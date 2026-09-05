/*
 * The virtio-gl-hyper fixed-function layer, as seen from Widelands.
 *
 * That layer implements OpenGL 1.x and is what Hexen II, TyrQuake, Quake III,
 * OpenGW and GLExcess render through. Widelands speaks OpenGL 2.1 and brings
 * its own implementation of that ABI in gl_frontend.cc, so fifteen names --
 * glDrawArrays, glBindTexture, glTexImage2D and friends -- exist in both.
 *
 * The layer is therefore compiled with virtgl_rename.h forced in front of it,
 * which links its entry points as wlgl_*. Not one line of it changes: five
 * other programs run on those exact files, and today has shown more than once
 * how far a change in there reaches.
 *
 * This header is the other side of that rename. gl_frontend.cc keeps the real
 * GL names, because that is what Widelands calls, and reaches the layer
 * through the declarations below.
 */
#ifndef WL_GRAPHIC_VIRTIO_VIRTGL_BRIDGE_H
#define WL_GRAPHIC_VIRTIO_VIRTGL_BRIDGE_H

#include "graphic/virtio/gl_api.h"

struct SDL_Window;

extern "C" {

/* Immediate mode. Widelands has no immediate-mode geometry of its own -- it
   builds vertex arrays for a shader -- so gl_frontend.cc walks those arrays
   and feeds them through here, one vertex at a time. That is what the
   translation is: the shaders it replaces are all "texture times colour",
   which is what this layer does natively. */
void wlgl_glBegin(GLenum mode);
void wlgl_glEnd(void);
void wlgl_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void wlgl_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void wlgl_glTexCoord2f(GLfloat s, GLfloat t);
void wlgl_glMultiTexCoord2fARB(GLenum unit, GLfloat s, GLfloat t);
void wlgl_glActiveTextureARB(GLenum unit);

/* Textures. The frontend keeps its own record of size and format for the
   queries Widelands makes; the pixels live here. */
void wlgl_glGenTextures(GLsizei count, GLuint* textures);
void wlgl_glDeleteTextures(GLsizei count, const GLuint* textures);
void wlgl_glBindTexture(GLenum target, GLuint texture);
void wlgl_glTexImage2D(GLenum target, GLint level, GLint internal_format,
                       GLsizei width, GLsizei height, GLint border,
                       GLenum format, GLenum type, const GLvoid* pixels);
void wlgl_glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y,
                          GLsizei width, GLsizei height, GLenum format,
                          GLenum type, const GLvoid* pixels);
void wlgl_glTexParameteri(GLenum target, GLenum name, GLint value);
void wlgl_glGetIntegerv(GLenum name, GLint* value);

/* Fixed state. */
void wlgl_glEnable(GLenum capability);
void wlgl_glDisable(GLenum capability);
void wlgl_glBlendFunc(GLenum source, GLenum destination);
void wlgl_glClear(GLbitfield mask);
void wlgl_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void wlgl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void wlgl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void wlgl_glMatrixMode(GLenum mode);
void wlgl_glLoadIdentity(void);
void wlgl_glDepthMask(GLboolean flag);
/* Widelands maps its z-layers onto [1, -1] and its first items land within
   one part in 65535 of the far plane, which is why it asks for GL_LEQUAL.
   The frontend accepted the call and dropped it: the layer was never told,
   and kept its own GL_LESS. */
void wlgl_glDepthFunc(GLenum func);
void wlgl_glTexEnvf(GLenum target, GLenum name, GLfloat value);

/* The frame boundary, in the order every port performs it. */
void wlgl_virtglPreparePresent(void);
void wlgl_virtglEndFrameCapture(void);

}  // extern "C"

/* The backend below the layer. These names carry no gl prefix, so the rename
   leaves them alone -- but they are extern "C" on its side, so they have to
   be declared that way here too or the reference is to a mangled name that
   does not exist. */
extern "C" {
bool virtioBackendOpen();
void virtioBackendClose();
bool virtioBackendReady();
bool virtioBackendCreateFrameTarget(struct Window* window, int width, int height);
void virtioBackendDestroyFrameTarget();
bool virtioBackendPresent();

/* SDL and Intuition meet in exactly one place; this is it. */
struct Window* virtioWindowFromSDL(SDL_Window* window);

/* What the backend did with the frame: how many batches it was handed, how
   many it drew, how many vertices, whether the submit failed and whether the
   target was lost. The other ports print this on a key; Widelands has no
   such key, and without it a frame that vanishes inside the backend is
   indistinguishable from one that was never submitted. */
unsigned virtioBackendStatus(char* out, unsigned size);

/* Where the backend writes its own log. The default is TyrQuake's file,
   which is where every Widelands run has been reporting its worker stalls.
   The path is kept as a pointer, so it must outlive the call. */
void virtioBackendSetLogFile(const char* path);
}  // extern "C"

#endif  // WL_GRAPHIC_VIRTIO_VIRTGL_BRIDGE_H
