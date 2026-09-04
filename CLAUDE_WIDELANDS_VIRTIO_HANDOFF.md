# Widelands AmigaOS4 direct VirtIO/VirGL handoff

## Goal

Finish the AmigaOS4 graphics port so Widelands renders through the public
`virtio_gpu.library` API without opening or linking Warp3DNova, MiniGL, gl4es,
Wazp3D, or any other native OpenGL implementation.

The required runtime path is:

```text
Widelands GL 2.1 calls
  -> compiled-in GL state tracker / translator
  -> VirGL command encoder
  -> LIBS:virtio_gpu.library
  -> QEMU virtio-gpu/virgl host
```

SDL2 is still used for video-window creation and input, but the AmigaOS4
VirtIO build must create a plain SDL window. It must not ask SDL for an OpenGL
window or context.

Sound is out of scope for now. Networking and ICU are deliberately disabled.
The port uses AmigaOS4 newlib, not clib2 or clib4.

## Current failure and root cause

The current Widelands binary opens `virtio_gpu.library`, but then still follows
the SDL OpenGL path in `src/graphic/graphic.cc` and
`src/graphic/gl/initialize.cc`:

- `SDL_GL_LoadLibrary(nullptr)`
- `SDL_WINDOW_OPENGL`
- `SDL_GL_CreateContext()`
- `SDL_GL_MakeCurrent()`
- `SDL_GL_SetSwapInterval()`
- `SDL_GL_SwapWindow()`
- `SDL_GL_DeleteContext()` / `SDL_GL_UnloadLibrary()`

On AmigaOS4 this enters gl4es and attempts to open `Warp3DNova.library`. That
is explicitly wrong for this port.

## Proven reference implementations

Use the current local OpenGW and TyrQuake sources as the architectural
reference. Do not base the port on their unmodified upstream SDL GL paths.

### OpenGW

Root:

```text
C:\Users\degro\Documents\Codex\2026-08-21\nie\work\virtio-gpu-virgl-os4\opengw-virtio
```

Important files:

- `OpenGW.cpp`: under `USE_VIRTGL`, creates a plain SDL window, calls
  `virtioBackendCreateFrameTarget()`, and never creates an SDL GL context.
- `virtgl_compat.cpp`: compiled-in GL state/call capture layer.
- `virtgl_gl.h`: locally supplied GL ABI.
- `virtio_backend.cpp/.h`: complete public-library lifecycle, resources,
  resident session, submission, optional readback, and direct scanout.
- `makefile.virtio.os4`: shows the direct build. It links SDL2 and pthread but
  no GL library.
- `README_VIRTIO_TEST.txt`: documents that GL4ES, MiniGL, Warp3D and Wazp3D
  are not linked or loaded.

### TyrQuake

Packaged source root:

```text
C:\Users\degro\Documents\Codex\2026-08-21\nie\work\virtio-gpu-virgl-os4\_stage_main_0251_6324492\source\glquake-port
```

Important files:

- `vid_virtio.c`: replaces TyrQuake's `vid_sgl.c`. It creates a plain
  `SDL_WINDOW_SHOWN` window, calls `virtioBackendCreateFrameTarget()`, then
  calls `virtglPreparePresent()` and `virtioBackendPresent()` at the normal
  swap point.
- `GL/gl.h` and `GL/glext.h`: redirect engine GL includes to the compiled-in
  compatibility API rather than the SDK GL implementation.
- `Makefile`: builds OpenGW's `virtgl_compat.cpp`, `virtio_backend.cpp`, and
  the driver's `virgl_encoder.c` into TyrQuake. It links no GL library.
- `README.md`: explicitly explains the no-real-GL-context design.

The OpenGW compatibility layer was expanded while bringing up TyrQuake, so
use the newest `opengw-virtio` files above rather than an old packaged copy.

## Important difference: Widelands is shader based

OpenGW and TyrQuake mainly exercise fixed-function/immediate-mode OpenGL.
Widelands uses desktop OpenGL 2.1 with GLSL 1.20, vertex buffers, vertex
attributes, textures, FBOs, and uniforms. The OpenGW/TyrQuake architecture and
backend are reusable, but their `virtgl_compat.cpp` cannot simply be copied and
declared complete.

The Widelands GL calls presently declared in
`src/graphic/virtio/gl_api.h` include:

- shaders/programs: create, source, compile, attach, link, use, info/status;
- uniforms: `glUniform1f`, `glUniform1i`, `glUniform2f`;
- VBOs and attributes: buffers, `glBufferData`, attribute locations,
  `glVertexAttribPointer`, enable/disable attributes;
- textures: allocation/upload, parameters, multiple texture units;
- FBOs: create/bind, attach texture, completeness, draw buffer;
- state: viewport, scissor, blend, depth, clear;
- drawing/readback: `glDrawArrays`, `glReadPixels`, `glGetTexImage`.

Before implementation, generate a definitive symbol inventory from the
Widelands sources/build. Do not assume the current hand-written header is
complete.

## Current Widelands port files

- `src/graphic/virtio/driver_api.h`: local copy of the stable public
  `virtio_gpu.library` ABI.
- `src/graphic/virtio/virgl_encoder.c/.h`: VirGL command encoder copied from
  the driver project.
- `src/graphic/virtio/virtio_gl.cc/.h`: incomplete bootstrap. It currently
  opens the driver, creates a context and two colour resources, starts a
  resident session, and submits an initial framebuffer clear. It is not a
  complete GL implementation or presenter.
- `src/graphic/virtio/gl_api.h`: minimal local OpenGL ABI declarations.
- `src/graphic/gl/system_headers.h`: selects the local ABI for
  `WL_AMIGAOS4_VIRTIO_GL`.
- `src/graphic/graphic.cc`: window creation, resize and swap integration.
- `src/graphic/gl/initialize.cc`: currently still creates the unwanted SDL GL
  context.
- `cmake/toolchains/AmigaOS4.cmake`: still points `OpenGL::GL` at
  `libgl4es.a`; this must disappear from the final direct VirtIO link.

Driver/reference project root:

```text
C:\Users\degro\Documents\Codex\2026-08-21\nie\work\virtio-gpu-virgl-os4
```

Keep the Widelands copy of public ABI/encoder files synchronized with the
current driver project. Do not invent private library entry points.

## Required implementation plan

### 1. Remove the native GL path on AmigaOS4 only

Under `WL_AMIGAOS4_VIRTIO_GL`:

- do not call `SDL_GL_LoadLibrary()`;
- create the SDL window without `SDL_WINDOW_OPENGL`;
- do not call any `SDL_GL_*Context`, swap interval, swap-window, or unload
  function;
- make the stored SDL GL context null/unused, or introduce a clean backend
  abstraction rather than faking an SDL context;
- leave non-AmigaOS4 Widelands behavior unchanged.

Use TyrQuake's `vid_virtio.c` split as the model.

### 2. Reuse the proven backend lifecycle

Bring Widelands' `virtio_gl` lifecycle in line with the newest OpenGW
`virtio_backend`:

- open `LIBS:virtio_gpu.library` version 2;
- obtain and validate only public interfaces/capabilities;
- open a client and create a VirGL context;
- create all resources before starting the resident session;
- keep resource/relocation arrays valid for each submission;
- support two colour targets and the public scanout interface;
- submit through resident APIs and fence correctly;
- present via double-buffered scanout when available;
- retain a correct readback/window-surface fallback only if direct scanout is
  unavailable;
- shut down in strict reverse order and handle partial initialization safely;
- recreate the frame target safely on resize.

Prefer adapting/copying proven backend code over rewriting subtle lifecycle,
worker, fence, readback, and scanout logic from memory. Preserve compatible
licensing notices.

### 3. Implement a Widelands GL 2.1 frontend

Implement the local `gl*` symbols in compiled-in source files. The frontend
must maintain the OpenGL object/state model expected by Widelands and emit
VirGL commands through the encoder/backend.

Minimum areas:

1. GL object ID allocation and lifetime for buffers, textures, shaders,
   programs, and framebuffers.
2. Buffer storage/upload and vertex attribute descriptions, including byte
   offsets passed to `glVertexAttribPointer` while a VBO is bound.
3. Texture units, texture upload/update, sampling parameters and sampler
   views.
4. Shader source ownership, compile/link status and logs.
5. Translation of Widelands GLSL 1.20 shaders into the shader representation
   accepted by VirGL. Do not falsely return successful compile/link status if
   no usable shader was created.
6. Attribute and uniform location mapping consistent across program use and
   draw submission.
7. Blend, depth, scissor, viewport, clear and framebuffer state.
8. `glDrawArrays(GL_TRIANGLES, ...)` using the active program, vertex layout,
   textures, uniforms, render target and state.
9. Offscreen framebuffer rendering used by Widelands textures/surfaces.
10. Error semantics and query values sufficient for Widelands startup checks.

The existing encoder already contains helpers for surfaces, framebuffers,
vertex elements/buffers, inline writes, shader text, samplers, constant
buffers, blend/DSA/rasterizer, viewport and draws. Extend it from the current
driver source when necessary rather than emitting unexplained magic packets
inside application code.

### 4. Replace swap with direct presentation

At both current `SDL_GL_SwapWindow()` sites in `graphic.cc`, call a Widelands
VirtIO present operation instead. The operation must flush/capture pending GL
state, submit the frame, wait/rotate as required by the public API, and present
the correct colour resource.

Do not present an independently cleared resource while Widelands is still
rendering somewhere else. The resource attached to the frontend's default
framebuffer must be the resource presented to scanout.

### 5. Remove gl4es from the final link

Once every required GL symbol is supplied by the compiled-in frontend:

- change the AmigaOS4 CMake/toolchain logic so `libgl4es.a` is not linked;
- retain the local GL headers to avoid MiniGL namespace/type collisions;
- inspect the final binary/link map and verify there are no unresolved or
  accidentally imported GL symbols.

## Data paths and runtime requirements

This is AmigaOS4, not Unix or Windows:

- program data lives at `PROGDIR:data`;
- all bundled data must be found relative to `PROGDIR:`;
- never transform this into `/PROGDIR`, `/Werk+Games:` or another POSIX path;
- AmigaDOS volume syntax is `volume:path`, not `/volume/path`;
- debug stdout/stderr goes to `shared:widelands/widelands.out`.

The current filesystem port already contains AmigaDOS path handling. Preserve
it while changing graphics code.

## Build/version/export rules

Build from:

```text
C:\Users\degro\Documents\ChatGPT\Widelands
```

The standard command is:

```powershell
.\build-amigaos4.ps1 -Action build
```

It uses Docker image `walkero/amigagccondocker:os4-gcc11` and increments the
patch number in `amigaos4-build-version.txt` at the start of every requested
build. Do not run it casually for configure-only experiments because each
`build` action consumes the next version number. Use `-Action configure` or a
manual incremental `cmake --build` while iterating when no distributable build
was requested.

Every distributable binary must contain both:

- Widelands `--version` output with the AmigaOS4 port version and build date;
- an embedded AmigaOS `$VER:` tag, e.g.
  `$VER: Widelands 0.0.3 (31.08.2026)`.

Strip and export the completed binary to:

```text
C:\Users\degro\Documents\Kyvos\shared\widelands\Widelands
```

Report its size and MD5 after every exported build.

Last known exported build before this work:

```text
version: 0.0.3
size:    25984600 bytes
MD5:     207e09ab5e144fe829133b87c38562e1
```

## Acceptance criteria

A build is not complete merely because it links. Verify on the AmigaOS4
target/emulator:

1. Startup output contains no gl4es initialization and no attempt to open
   `Warp3DNova.library`, MiniGL, or Wazp3D.
2. The only graphics library opened by the Widelands graphics backend is
   `LIBS:virtio_gpu.library` through its public API.
3. A plain SDL window is created without an SDL GL context.
4. Widelands passes its OpenGL version, GLSL version, maximum texture size,
   shader compile and program link checks honestly.
5. The initial menu renders correctly rather than only showing a clear colour.
6. Textures, text, blending, scissoring and offscreen surfaces are correct.
7. Resize/recreate and shutdown do not leak, hang, or use destroyed resources.
8. Direct scanout visibly alternates the intended frame resources without
   tearing/stale frames; fallback presentation works if direct scanout is not
   available.
9. `PROGDIR:data` is found regardless of the current shell directory.
10. `shared:widelands/widelands.out` contains useful staged diagnostics on
    failure.

Add concise milestone logs so failures can be located from the target log:
library/interfaces, capabilities, client, context, resources, resident session,
frontend readiness, first successful shader/program, first draw, first
successful present, resize, and orderly shutdown.

## Working-tree caution

The Widelands tree already contains many deliberate uncommitted AmigaOS4 port
changes. Preserve them. Do not reset, revert, reformat, or overwrite unrelated
files. Inspect the current diff before editing and keep changes scoped to this
graphics task.

## Recommended first milestone

First produce a diagnostic build that:

- creates a plain SDL window;
- does not link/load gl4es or Nova;
- opens the public VirtIO interfaces;
- creates the actual default framebuffer resources;
- submits and presents a clear through direct scanout;
- exits cleanly.

Then implement GL calls in coherent groups and exercise them with small
preflight paths before attempting the entire Widelands menu. This matches the
successful OpenGW/TyrQuake bring-up strategy while respecting Widelands'
substantially larger shader-based GL surface.
