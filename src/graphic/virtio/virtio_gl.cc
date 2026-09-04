#include "graphic/virtio/virtio_gl.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <SDL_surface.h>
#include <SDL_video.h>

#include "base/log.h"

#if defined(WL_AMIGAOS4_VIRTIO_GL) && defined(__amigaos4__)
#include "graphic/virtio/driver_api.h"
extern "C" {
#include "graphic/virtio/virgl_encoder.h"
}

#include <exec/libraries.h>
#include <proto/exec.h>

namespace {

Library* library = nullptr;
vgpu_driver_iface* driver = nullptr;
vgpu_resident_iface* resident_driver = nullptr;
vgpu_scanout_iface* scanout_driver = nullptr;
vgpu_client_handle client = 0;
vgpu_context_handle context = 0;
vgpu_resident_handle resident = 0;
vgpu_resource_handle colour_buffers[2] = {0, 0};
uint32_t target_width = 0;
uint32_t target_height = 0;
uint32_t present_slot = 0;
bool scanout_active = false;
SDL_Window* output_window = nullptr;
std::vector<uint8_t> readback_pixels;
std::vector<uint8_t> flipped_pixels;
std::string error;

constexpr uint32_t kColourMarkerBase = UINT32_C(0xfeed0001);

bool submit_framebuffer_setup() {
	uint8_t commands[256]{};
	vgpu_virgl_encoder encoder;
	vgpu_virgl_encoder_init(&encoder, commands, sizeof(commands));
	vgpu_virgl_create_surface(&encoder, 1, kColourMarkerBase, 1);
	vgpu_virgl_create_surface(&encoder, 2, kColourMarkerBase + 1, 1);
	vgpu_virgl_set_framebuffer(&encoder, 1);
	const uint32_t black[4] = {0, 0, 0, UINT32_C(0x3f800000)};
	vgpu_virgl_clear(&encoder, 4, black);
	if (!vgpu_virgl_encoder_ok(&encoder)) {
		return false;
	}

	vgpu_resource_relocation relocations[2]{};
	uint32_t relocation_count = 0;
	for (uint32_t offset = 0; offset + 3 < vgpu_virgl_encoder_size(&encoder); offset += 4) {
		const uint32_t value = static_cast<uint32_t>(commands[offset]) |
		                       (static_cast<uint32_t>(commands[offset + 1]) << 8) |
		                       (static_cast<uint32_t>(commands[offset + 2]) << 16) |
		                       (static_cast<uint32_t>(commands[offset + 3]) << 24);
		if (value >= kColourMarkerBase && value < kColourMarkerBase + 2) {
			relocations[relocation_count].offset = offset;
			relocations[relocation_count].binding = value - kColourMarkerBase;
			++relocation_count;
		}
	}

	vgpu_submit_desc submission{};
	submission.struct_size = sizeof(submission);
	submission.commands = commands;
	submission.command_size = vgpu_virgl_encoder_size(&encoder);
	submission.resources = colour_buffers;
	submission.resource_count = 2;
	submission.relocations = relocations;
	submission.relocation_count = relocation_count;
	submission.command_repeat_count = 1;
	vgpu_fence_handle fence = 0;
	if (relocation_count != 2 ||
	    resident_driver->SubmitResident(
	       resident_driver, client, resident, &submission, &fence) < 0) {
		return false;
	}
	uint32_t status = VGPU_FENCE_PENDING;
	const bool completed = driver->WaitFence(driver, client, fence, 0, &status) &&
	                       status == VGPU_FENCE_SIGNALED;
	if (fence != 0) {
		driver->DestroyFence(driver, client, fence);
	}
	return completed;
}

void fail(const char* message) {
	error = message;
	log_err("VirtIO GL: %s\n", error.c_str());
}

bool present_readback_surface(uint32_t slot) {
	if (output_window == nullptr) {
		fail("readback fallback has no SDL output window");
		return false;
	}
	const size_t row_bytes = static_cast<size_t>(target_width) * 4U;
	const size_t byte_count = row_bytes * target_height;
	readback_pixels.resize(byte_count);
	if (!driver->ReadResource(
	       driver, client, colour_buffers[slot], 0, readback_pixels.data(), byte_count)) {
		fail("could not read back the VirtIO colour buffer");
		return false;
	}

	SDL_Surface* surface = SDL_GetWindowSurface(output_window);
	if (surface == nullptr) {
		fail("could not acquire the SDL window surface for readback presentation");
		return false;
	}
	/* VirGL readback is bottom-up, while SDL window surfaces are top-down. */
	flipped_pixels.resize(byte_count);
	for (uint32_t y = 0; y < target_height; ++y) {
		std::memcpy(flipped_pixels.data() + static_cast<size_t>(target_height - 1U - y) * row_bytes,
		            readback_pixels.data() + static_cast<size_t>(y) * row_bytes, row_bytes);
	}
	if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0) {
		fail("could not lock the SDL window surface for readback presentation");
		return false;
	}
	const int conversion = SDL_ConvertPixels(
	   static_cast<int>(target_width), static_cast<int>(target_height), SDL_PIXELFORMAT_BGRA32,
	   flipped_pixels.data(), static_cast<int>(row_bytes), surface->format->format, surface->pixels,
	   surface->pitch);
	if (SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}
	if (conversion != 0 || SDL_UpdateWindowSurface(output_window) != 0) {
		fail("could not update the SDL window surface from VirtIO readback");
		return false;
	}
	static bool reported = false;
	if (!reported) {
		log_info("VirtIO GL: direct scanout unavailable; using synchronous resident readback to SDL surface\n");
		reported = true;
	}
	return true;
}

}  // namespace

namespace VirtioGl {

void set_output_window(SDL_Window* window) {
	output_window = window;
}

bool initialize_driver() {
	if (driver_ready()) {
		return true;
	}

	error.clear();
	library = IExec->OpenLibrary("PROGDIR:virtio_gpu.library", 2);
	if (library == nullptr) {
		library = IExec->OpenLibrary("LIBS:virtio_gpu.library", 2);
	}
	if (library == nullptr) {
		fail("cannot open PROGDIR:virtio_gpu.library or LIBS:virtio_gpu.library version 2");
		return false;
	}

	driver = reinterpret_cast<vgpu_driver_iface*>(IExec->GetInterface(library, "driver", 1, nullptr));
	if (driver == nullptr) {
		fail("driver interface version 1 is unavailable");
		shutdown_driver();
		return false;
	}

	vgpu_capabilities capabilities{};
	capabilities.struct_size = sizeof(capabilities);
	if (driver->GetAPIVersion(driver) != VGPU_API_VERSION ||
	    !driver->GetCapabilities(driver, &capabilities)) {
		fail("driver API/capability query failed");
		shutdown_driver();
		return false;
	}
	if ((capabilities.flags & VGPU_CAP_VIRGL) == 0) {
		fail("the VirtIO GPU does not advertise VirGL support");
		shutdown_driver();
		return false;
	}
	if ((capabilities.flags & VGPU_CAP_RESIDENT_SESSION) == 0) {
		fail("the VirtIO GPU does not support resident sessions");
		shutdown_driver();
		return false;
	}
	resident_driver = reinterpret_cast<vgpu_resident_iface*>(
	   IExec->GetInterface(library, "resident", 1, nullptr));
	if (resident_driver == nullptr) {
		fail("resident interface version 1 is unavailable");
		shutdown_driver();
		return false;
	}
	if ((capabilities.flags & VGPU_CAP_DOUBLE_BUFFERED_SCANOUT) != 0) {
		scanout_driver = reinterpret_cast<vgpu_scanout_iface*>(
		   IExec->GetInterface(library, "scanout", 1, nullptr));
	}
	if (!driver->OpenClient(driver, &client)) {
		fail("driver refused a Widelands client");
		shutdown_driver();
		return false;
	}

	log_info("VirtIO GL: library %u.%u, API %u, caps 0x%08x, client 0x%08x\n",
	         static_cast<unsigned>(library->lib_Version),
	         static_cast<unsigned>(library->lib_Revision), capabilities.api_version,
	         capabilities.flags, client);
	return true;
}

void shutdown_driver() {
	destroy_frame_target();
	if (driver != nullptr && client != 0) {
		driver->CloseClient(driver, client);
	}
	client = 0;
	if (scanout_driver != nullptr) {
		IExec->DropInterface(reinterpret_cast<Interface*>(scanout_driver));
	}
	scanout_driver = nullptr;
	if (resident_driver != nullptr) {
		IExec->DropInterface(reinterpret_cast<Interface*>(resident_driver));
	}
	resident_driver = nullptr;
	if (driver != nullptr) {
		IExec->DropInterface(reinterpret_cast<Interface*>(driver));
	}
	driver = nullptr;
	if (library != nullptr) {
		IExec->CloseLibrary(library);
	}
	library = nullptr;
}

bool driver_ready() {
	return library != nullptr && driver != nullptr && client != 0;
}

bool create_frame_target(int width, int height) {
	if (!driver_ready() || width <= 0 || height <= 0) {
		fail("invalid state or dimensions for the frame target");
		return false;
	}
	if (frame_target_ready() && target_width == static_cast<uint32_t>(width) &&
	    target_height == static_cast<uint32_t>(height)) {
		return true;
	}

	destroy_frame_target();
	const uint64_t backing_size = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4;
	if (backing_size > UINT32_MAX) {
		fail("frame target is too large for the driver ABI");
		return false;
	}
	if (!driver->CreateContext(driver, client, &context)) {
		fail("could not create a VirGL context");
		return false;
	}

	vgpu_resource_desc description{};
	description.struct_size = sizeof(description);
	description.type = VGPU_RESOURCE_2D;
	description.width = static_cast<uint32_t>(width);
	description.height = static_cast<uint32_t>(height);
	description.depth = 1;
	description.format = 1;      // PIPE_FORMAT_B8G8R8A8_UNORM
	description.bind_flags = 2;  // VIRGL_BIND_RENDER_TARGET
	description.backing_size = static_cast<uint32_t>(backing_size);
	description.target = 2;  // PIPE_TEXTURE_2D
	description.array_size = 1;

	for (vgpu_resource_handle& colour_buffer : colour_buffers) {
		if (!driver->CreateResource(driver, client, &description, &colour_buffer)) {
			fail("could not create both VirGL colour buffers");
			destroy_frame_target();
			return false;
		}
	}
	if (resident_driver->BeginResident(resident_driver, client, context, colour_buffers, 2,
	                                   nullptr, nullptr, &resident) != 0) {
		fail("could not begin the resident VirGL frame session");
		destroy_frame_target();
		return false;
	}
	if (!submit_framebuffer_setup()) {
		fail("could not submit the initial VirGL framebuffer state");
		destroy_frame_target();
		return false;
	}
	target_width = description.width;
	target_height = description.height;
	log_info("VirtIO GL: context 0x%08x, double frame target %ux%u (%u bytes each)\n",
	         context, target_width, target_height, description.backing_size);
	return true;
}

void destroy_frame_target() {
	if (driver != nullptr && client != 0) {
		if (resident != 0 && scanout_driver != nullptr && scanout_active) {
			scanout_driver->DisableScanoutResident(
			   scanout_driver, client, resident, 0, nullptr, nullptr);
		}
		scanout_active = false;
		if (resident != 0 && resident_driver != nullptr) {
			resident_driver->EndResident(resident_driver, client, resident, nullptr, nullptr);
		}
		resident = 0;
		for (vgpu_resource_handle& colour_buffer : colour_buffers) {
			if (colour_buffer != 0) {
				driver->DestroyResource(driver, client, colour_buffer);
			}
			colour_buffer = 0;
		}
		if (context != 0) {
			driver->DestroyContext(driver, client, context);
		}
	}
	context = 0;
	target_width = 0;
	target_height = 0;
	present_slot = 0;
	readback_pixels.clear();
	flipped_pixels.clear();
}

bool frame_target_ready() {
	return driver_ready() && context != 0 && resident != 0 && colour_buffers[0] != 0 &&
	       colour_buffers[1] != 0;
}

bool present() {
	if (!frame_target_ready()) {
		fail("cannot present without a ready frame target");
		return false;
	}
	const bool use_readback_fallback = scanout_driver == nullptr;

	/* Keep presentation in the same resident submission stream as rendering.
	 * For now this command only selects the surface that the compiled-in GL
	 * frontend has rendered into.  Subsequent frontend work will prepend the
	 * actual frame commands instead of using an SDL OpenGL swap. */
	uint8_t commands[64]{};
	vgpu_virgl_encoder encoder;
	vgpu_virgl_encoder_init(&encoder, commands, sizeof(commands));
	vgpu_virgl_set_framebuffer(&encoder, present_slot + 1);
#if defined(WL_AMIGAOS4_VIRTIO_NO_SHADERS)
	/* The current diagnostic frontend accepts Widelands' GL commands but does
	 * not yet encode its shader-based draw calls.  Keep this clear in the real
	 * VirGL submission path so the target has an unambiguous visual proof that
	 * the resident double buffers and scanout presentation work.  Components
	 * are IEEE-754 floats: 0.10, 0.75, 0.90, 1.00 (bright cyan). */
	const uint32_t diagnostic_cyan[4] = {
	   UINT32_C(0x3dcccccd), UINT32_C(0x3f400000), UINT32_C(0x3f666666), UINT32_C(0x3f800000)};
	vgpu_virgl_clear(&encoder, 4, diagnostic_cyan);
#endif
	if (!vgpu_virgl_encoder_ok(&encoder)) {
		fail("could not encode the VirtIO presentation command");
		return false;
	}

	vgpu_submit_desc submission{};
	submission.struct_size = sizeof(submission);
	submission.commands = commands;
	submission.command_size = vgpu_virgl_encoder_size(&encoder);
	submission.resources = colour_buffers;
	submission.resource_count = 2;
	submission.command_repeat_count = 1;

	vgpu_fence_handle fence = 0;
	const int32_t submit_status = use_readback_fallback ?
	   resident_driver->SubmitReadbackResident(
	      resident_driver, client, resident, &submission, colour_buffers[present_slot], &fence) :
	   scanout_driver->SubmitScanoutResident(
	      scanout_driver, client, resident, &submission, colour_buffers[present_slot], 0, &fence);
	if (submit_status != 0) {
		if (fence != 0) {
			driver->DestroyFence(driver, client, fence);
		}
		fail(use_readback_fallback ? "VirtIO resident readback submission failed" :
	                             "VirtIO scanout submission failed");
		return false;
	}

	uint32_t fence_status = VGPU_FENCE_PENDING;
	const bool completed = driver->WaitFence(driver, client, fence, 0, &fence_status) &&
	                       fence_status == VGPU_FENCE_SIGNALED;
	if (fence != 0) {
		driver->DestroyFence(driver, client, fence);
	}
	if (!completed) {
		fail(use_readback_fallback ? "VirtIO resident readback fence did not complete successfully" :
	                             "VirtIO scanout fence did not complete successfully");
		return false;
	}

	if (use_readback_fallback && !present_readback_surface(present_slot)) {
		return false;
	}
	scanout_active = !use_readback_fallback;
	present_slot ^= 1;
	return true;
}

const char* last_error() {
	return error.empty() ? "unknown error" : error.c_str();
}

}  // namespace VirtioGl

#else

namespace VirtioGl {

void set_output_window(SDL_Window*) {
}

bool initialize_driver() {
	return false;
}
void shutdown_driver() {
}
bool driver_ready() {
	return false;
}
bool create_frame_target(int, int) {
	return false;
}
void destroy_frame_target() {
}
bool frame_target_ready() {
	return false;
}
bool present() {
	return false;
}
const char* last_error() {
	return "VirtIO GL was not compiled for AmigaOS4";
}

}  // namespace VirtioGl

#endif
