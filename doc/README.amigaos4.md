# AmigaOS4 port status

The cross-build uses `walkero/amigagccondocker:os4-gcc11` and the newlib SDK.
Run `./build-amigaos4.ps1 configure` to probe dependencies and
`./build-amigaos4.ps1 build` for a full build.

Sound is not part of the first runtime milestone. Until the sound target is
stubbed out completely, SDL2_mixer remains a link-time dependency; launch the
result with `--nosound`.

The AmigaOS4 build does not use clib2, clib4 or ICU. A small embedded
newlib-compatible UTF-8/UTF-16 converter supplies the subset of UnicodeString
needed by Widelands' existing text, word-wrap and BiDi code.

Standalone Asio is used header-only from `auto_dependencies/asio/asio/include`;
it does not introduce another C runtime.

## VirtIO/VirGL integration blocker

Widelands requires desktop OpenGL 2.1, GLSL 1.20, framebuffer objects, buffer
objects, multitexturing and runtime entry-point loading through
`SDL_GL_GetProcAddress`. The current `virtio_gpu.library` driver API and
OpenGW compatibility layer do not yet provide that complete interface. In
particular, Widelands uses shader/program and framebuffer entry points that are
not exported by `virtgl_compat`.

The first compiled-in integration milestone now opens `virtio_gpu.library`,
validates driver API v1 and `VGPU_CAP_VIRGL`, and owns a driver client for the
entire graphics lifetime. It also creates one VirGL context and two BGRA8
render-target resources sized to the SDL window, recreates them after window or
fullscreen size changes, and rolls back on partial creation failure. The
temporary rendering path remains SDL2 plus gl4es;
framebuffer rendering does not use the wrapper yet. The wrapper path
becomes viable after either:

1. an SDL2 OpenGL backend creates and swaps a VirtIO-backed context while
   `SDL_GL_GetProcAddress` exposes all required GL 2.1 functions; or
2. a separate `minigl_virtio.library` supplies those semantics.

Keep `virtio_gpu.library` separate from system MiniGL/Warp3D during development.

The local driver-v1 ABI currently ends at `DestroyResource`. Connecting the
VirGL encoder requires the matching stable ABI declarations for command submit,
resource transfer/attach, fences and presentation. Do not guess their interface
member order: AmigaOS4 interface calls depend on an exact binary layout.
