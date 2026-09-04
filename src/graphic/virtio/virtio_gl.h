#ifndef WL_GRAPHIC_VIRTIO_VIRTIO_GL_H
#define WL_GRAPHIC_VIRTIO_VIRTIO_GL_H

struct SDL_Window;

namespace VirtioGl {

// Opens and validates the transport used by the compiled-in GL frontend.
bool initialize_driver();
void shutdown_driver();
bool driver_ready();
// The plain SDL window is only used when the public direct-scanout interface
// is absent and the resident readback fallback must present a frame.
void set_output_window(SDL_Window* window);
bool create_frame_target(int width, int height);
void destroy_frame_target();
bool frame_target_ready();
// Submits the current default framebuffer and presents it through scanout 0.
// The compiled-in GL frontend will append its frame commands before this is
// used as the replacement for SDL_GL_SwapWindow().
bool present();
const char* last_error();

}  // namespace VirtioGl

#endif  // WL_GRAPHIC_VIRTIO_VIRTIO_GL_H
