/*
 * Copyright (C) 2026 by the Widelands Development Team
 *
 * An adapter over the virtio-gl-hyper backend, keeping the interface
 * graphic.cc already calls.
 *
 * This file used to be a second, thinner implementation of what that backend
 * does: its own driver handshake, its own resident session, its own resources,
 * its own present -- and its own snapshot of the VirGL encoder, months behind.
 * Two implementations of the same thing means two places to fix everything,
 * and the older one was already missing the mip levels, the blend factor
 * table and the rasterizer modes the newer one had grown.
 *
 * So the session belongs to virtio_backend.cpp now, which is the one that
 * runs Hexen II, TyrQuake, Quake III, OpenGW and GLExcess. What is left here
 * is the shape graphic.cc expects wrapped around it, and the one place where
 * an SDL window becomes an Intuition window.
 */

#include "graphic/virtio/virtio_gl.h"

#ifdef WL_AMIGAOS4_VIRTIO_GL

#include <cstdlib>
#include <string>

#include <SDL_video.h>

#include "base/log.h"

#include "graphic/virtio/virtgl_bridge.h"

namespace {

std::string last_error_text;
SDL_Window* output_window = nullptr;
bool driver_open = false;
bool target_ready = false;
int target_width = 0;
int target_height = 0;

void fail(const char* message) {
	last_error_text = message;
}

}  // namespace

namespace VirtioGl {

void set_output_window(SDL_Window* window) {
	output_window = window;
}

bool initialize_driver() {
	if (driver_open) {
		return true;
	}
	/* Wait for the worker instead of dropping the frame.
	 *
	 * The backend does not pace by default: if the worker is still busy when
	 * present() is called, the frame is discarded and present() returns true
	 * anyway. That is the right trade for the engines it was tuned on --
	 * frames arrive continuously there, a dropped one is invisible, and the
	 * wait sits on the thread that reads the keyboard.
	 *
	 * Widelands is the opposite. It presents three times for the splash
	 * screen and then not again until the texture atlas is built, a minute
	 * and a half later. Every dropped frame is a frame nobody ever sees, and
	 * the log says "present -> ok" for all of them -- which is why a
	 * correctly rendered splash screen sat in the pipeline while the window
	 * stayed black, and why turning on debug logging (ten times slower, so
	 * the worker was always ready) appeared to fix the rendering.
	 *
	 * Set rather than forced: an explicit setting in the environment wins. */
	setenv("VIRTIOGL_PACING", "1", 0);
	/* The backend's log defaults to TyrQuake's file, so every Widelands run
	   so far has been writing its worker stalls into another game's log --
	   and this happens with debug logging off too, since the calls that
	   report a stalled worker or a lost frame target always write. A string
	   literal because the backend keeps the pointer. */
	virtioBackendSetLogFile("PROGDIR:widelands-virtgl.log");
	if (!virtioBackendOpen()) {
		fail("cannot open PROGDIR:virtio_gpu.library or LIBS:virtio_gpu.library version 2");
		return false;
	}
	driver_open = true;
	last_error_text.clear();
	log_info("VirtIO GL: backend open");
	return true;
}

void shutdown_driver() {
	if (!driver_open) {
		return;
	}
	destroy_frame_target();
	virtioBackendClose();
	driver_open = false;
}

bool driver_ready() {
	return driver_open && virtioBackendReady();
}

bool create_frame_target(int width, int height) {
	if (!driver_ready()) {
		fail("the driver is not open");
		return false;
	}
	if (output_window == nullptr) {
		fail("no output window has been set");
		return false;
	}
	/* A resize replaces the target rather than growing it: the session's
	   resources were sized for the old one. */
	if (target_ready) {
		virtioBackendDestroyFrameTarget();
		target_ready = false;
	}
	struct Window* window = virtioWindowFromSDL(output_window);
	if (window == nullptr) {
		fail("SDL did not report an Intuition window for this SDL_Window");
		return false;
	}
	if (!virtioBackendCreateFrameTarget(window, width, height)) {
		fail("the backend refused a frame target of that size");
		log_info("VirtIO GL: frame target %dx%d REFUSED", width, height);
		return false;
	}
	target_ready = true;
	target_width = width;
	target_height = height;
	log_info("VirtIO GL: frame target %dx%d ready", width, height);

	/* Widelands hands over positions that are already in clip space -- its
	   vertex shaders do nothing but pass attr_position through -- so both
	   matrices stay identity and the layer transforms nothing. */
	wlgl_glMatrixMode(GL_PROJECTION);
	wlgl_glLoadIdentity();
	wlgl_glMatrixMode(GL_MODELVIEW);
	wlgl_glLoadIdentity();
	wlgl_glViewport(0, 0, width, height);

	last_error_text.clear();
	return true;
}

void destroy_frame_target() {
	if (!target_ready) {
		return;
	}
	virtioBackendDestroyFrameTarget();
	target_ready = false;
	target_width = 0;
	target_height = 0;
}

bool frame_target_ready() {
	return target_ready;
}

bool present() {
	if (!target_ready) {
		fail("cannot present without a ready frame target");
		return false;
	}
	/* The order every port performs: hand the captured frame over, submit it,
	   then clear the capture for the next one. Doing it in any other order
	   drops a frame or replays the last one. */
	static unsigned frames = 0;
	wlgl_virtglPreparePresent();
	const bool presented = virtioBackendPresent();
	wlgl_virtglEndFrameCapture();
	if (!presented) {
		fail("the backend refused the frame");
	}
	/* The first one, and then rarely: a present that starts working and then
	   stops is a different problem from one that never worked. */
	if (frames == 0 || (frames % 300) == 0) {
		log_info("VirtIO GL: present %u -> %s", frames, presented ? "ok" : "refused");
	}
	/* The backend's own account of the frame, once it has had one to work
	   with. The status is many lines in one buffer, so it goes out a line at
	   a time -- a single log_info of the whole thing is what a 512-byte
	   buffer silently truncated last time, taking the geometry counts with
	   it. */
	/* Not frame 1: the backend pipelines, so present() collects the previous
	   frame and submits this one. Asked that early, every counter still
	   describes the empty frame the window was opened with -- which is how
	   "drawn=0" was read as "nothing was ever drawn". Frames 3 and 5 have a
	   completed frame behind them, and the 300s land in the main menu. */
	/* The splash is presented twice and then nothing happens for the length
	   of the atlas build, so frame 2 is the last chance to read a completed
	   frame before that silence -- and the first one that describes the
	   splash rather than the empty frame the window opened with. */
	if (frames == 2 || frames == 3 || (frames != 0 && (frames % 300) == 0)) {
		char status[8192];
		const unsigned written = virtioBackendStatus(status, sizeof(status));
		if (written != 0u) {
			status[sizeof(status) - 1] = '\0';
			char* line = status;
			while (*line != '\0') {
				char* end = line;
				while (*end != '\0' && *end != '\n') {
					++end;
				}
				const bool more = *end == '\n';
				*end = '\0';
				if (*line != '\0') {
					log_info("VirtIO GL: %s", line);
				}
				line = more ? end + 1 : end;
			}
		} else {
			log_info("VirtIO GL: backend reported no status");
		}
	}
	++frames;
	return presented;
}

const char* last_error() {
	return last_error_text.c_str();
}

}  // namespace VirtioGl

#endif  // WL_AMIGAOS4_VIRTIO_GL
