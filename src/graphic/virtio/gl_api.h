/* Minimal desktop OpenGL 2.1 ABI required by the compiled-in VirtIO frontend.
 * Do not include AmigaOS MiniGL: its global Image and Trackable collide with Widelands classes. */
#ifndef WL_GRAPHIC_VIRTIO_GL_API_H
#define WL_GRAPHIC_VIRTIO_GL_API_H

#include <cstddef>

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLvoid = void;
using GLbyte = signed char;
using GLshort = short;
using GLint = int;
using GLsizei = int;
using GLubyte = unsigned char;
using GLushort = unsigned short;
using GLuint = unsigned int;
using GLfloat = float;
using GLclampf = float;
using GLdouble = double;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NO_ERROR 0
#define GL_LINES 0x0001
/* Fixed-function matrix modes. Widelands never sets a matrix -- its vertex
   shaders pass positions through in clip space -- but the layer underneath
   has a matrix stack, so it is told to leave both identity. */
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TRIANGLES 0x0004
#define GL_LEQUAL 0x0203
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_BACK 0x0405
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_OUT_OF_MEMORY 0x0505
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_DOUBLEBUFFER 0x0C32
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_VERSION 0x1F02
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_TABLE_TOO_LARGE 0x8031
#define GL_INTENSITY 0x8049
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FUNC_ADD 0x8006
#define GL_ADD 0x0104
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER 0x8D40

extern "C" {
void glActiveTexture(GLenum);
void glAttachShader(GLuint, GLuint);
void glBindBuffer(GLenum, GLuint);
void glBindFramebuffer(GLenum, GLuint);
void glBindTexture(GLenum, GLuint);
void glBlendEquation(GLenum);
void glBlendFunc(GLenum, GLenum);
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum);
GLenum glCheckFramebufferStatus(GLenum);
void glClear(GLbitfield);
void glCompileShader(GLuint);
GLuint glCreateProgram(void);
GLuint glCreateShader(GLenum);
void glDeleteBuffers(GLsizei, const GLuint*);
void glDeleteFramebuffers(GLsizei, const GLuint*);
void glDeleteProgram(GLuint);
void glDeleteShader(GLuint);
void glDeleteTextures(GLsizei, const GLuint*);
void glDepthFunc(GLenum);
void glDisable(GLenum);
void glDisableVertexAttribArray(GLuint);
void glDrawArrays(GLenum, GLint, GLsizei);
void glDrawBuffer(GLenum);
void glEnable(GLenum);
void glEnableVertexAttribArray(GLuint);
void glFlush(void);
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
void glGenBuffers(GLsizei, GLuint*);
void glGenFramebuffers(GLsizei, GLuint*);
void glGenTextures(GLsizei, GLuint*);
GLint glGetAttribLocation(GLuint, const GLchar*);
void glGetBooleanv(GLenum, GLboolean*);
GLenum glGetError(void);
void glGetIntegerv(GLenum, GLint*);
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
void glGetProgramiv(GLuint, GLenum, GLint*);
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
void glGetShaderiv(GLuint, GLenum, GLint*);
const GLubyte* glGetString(GLenum);
void glGetTexImage(GLenum, GLint, GLenum, GLenum, void*);
GLint glGetUniformLocation(GLuint, const GLchar*);
void glLinkProgram(GLuint);
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
void glScissor(GLint, GLint, GLsizei, GLsizei);
void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void glTexParameteri(GLenum, GLenum, GLint);
void glUniform1f(GLint, GLfloat);
void glUniform1i(GLint, GLint);
void glUniform2f(GLint, GLfloat, GLfloat);
void glUseProgram(GLuint);
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
void glViewport(GLint, GLint, GLsizei, GLsizei);
}

#endif  // WL_GRAPHIC_VIRTIO_GL_API_H
