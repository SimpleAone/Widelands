/*
 * Copyright (C) 2026 by the Widelands Development Team
 *
 * First-stage OpenGL 2.1 state frontend for the AmigaOS4 VirtIO backend.
 * This translation unit deliberately owns the GL ABI so the direct backend
 * never falls through to gl4es/MiniGL.  Unsupported rendering operations fail
 * honestly until their VirGL translations are implemented.
 */

#include "graphic/virtio/gl_api.h"

#ifdef WL_AMIGAOS4_VIRTIO_GL

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

struct ShaderState {
	GLenum type{0};
	std::string source;
	std::string log;
	bool compiled{false};
};

struct ProgramState {
	std::set<GLuint> shaders;
	std::string log;
	bool linked{false};
	std::map<std::string, GLint> attributes;
	std::map<std::string, GLint> uniforms;
};

struct TextureState {
	GLsizei width{0};
	GLsizei height{0};
	GLint internal_format{0};
	std::vector<unsigned char> pixels;
};

struct BufferState {
	std::vector<unsigned char> bytes;
};

struct FramebufferState {
	GLuint colour_texture{0};
};

GLuint next_object = 1;
GLenum pending_error = GL_NO_ERROR;
GLuint active_texture_unit = 0;
GLuint bound_textures[2] = {0, 0};
GLuint bound_buffer = 0;
GLuint bound_framebuffer = 0;
GLuint current_program = 0;
std::map<GLuint, ShaderState> shaders;
std::map<GLuint, ProgramState> programs;
std::map<GLuint, TextureState> textures;
std::map<GLuint, BufferState> buffers;
std::map<GLuint, FramebufferState> framebuffers;

void set_error(GLenum value) {
	if (pending_error == GL_NO_ERROR) {
		pending_error = value;
	}
}

GLuint allocate_object() {
	return next_object++;
}

template <typename Map>
void generate_objects(GLsizei count, GLuint* output, Map* objects) {
	if (count < 0 || (count > 0 && output == nullptr)) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	for (GLsizei i = 0; i < count; ++i) {
		output[i] = allocate_object();
		objects->emplace(output[i], typename Map::mapped_type{});
	}
}

template <typename Map>
void delete_objects(GLsizei count, const GLuint* input, Map* objects) {
	if (count < 0 || (count > 0 && input == nullptr)) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	for (GLsizei i = 0; i < count; ++i) {
		objects->erase(input[i]);
	}
}

void copy_log(const std::string& source, GLsizei capacity, GLsizei* length, GLchar* output) {
	const GLsizei copied = capacity > 0 && output != nullptr ?
	                           std::min<GLsizei>(capacity - 1, source.size()) :
	                           0;
	if (copied > 0) {
		std::memcpy(output, source.data(), copied);
	}
	if (capacity > 0 && output != nullptr) {
		output[copied] = '\0';
	}
	if (length != nullptr) {
		*length = copied;
	}
}

}  // namespace

extern "C" {

void glActiveTexture(GLenum texture) {
	if (texture < GL_TEXTURE0 || texture > GL_TEXTURE1) {
		set_error(GL_INVALID_ENUM);
		return;
	}
	active_texture_unit = texture - GL_TEXTURE0;
}

void glAttachShader(GLuint program, GLuint shader) {
	if (programs.count(program) == 0 || shaders.count(shader) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	programs[program].shaders.insert(shader);
}

void glBindBuffer(GLenum target, GLuint buffer) {
	if (target != GL_ARRAY_BUFFER) {
		set_error(GL_INVALID_ENUM);
		return;
	}
	if (buffer != 0 && buffers.count(buffer) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	bound_buffer = buffer;
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
	if (target != GL_FRAMEBUFFER) {
		set_error(GL_INVALID_ENUM);
		return;
	}
	if (framebuffer != 0 && framebuffers.count(framebuffer) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	bound_framebuffer = framebuffer;
}

void glBindTexture(GLenum target, GLuint texture) {
	if (target != GL_TEXTURE_2D) {
		set_error(GL_INVALID_ENUM);
		return;
	}
	if (texture != 0 && textures.count(texture) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	bound_textures[active_texture_unit] = texture;
}

void glBlendEquation(GLenum) {
}
void glBlendFunc(GLenum, GLenum) {
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
	if (target != GL_ARRAY_BUFFER || usage != GL_DYNAMIC_DRAW) {
		set_error(GL_INVALID_ENUM);
		return;
	}
	if (bound_buffer == 0 || size < 0) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	auto& bytes = buffers[bound_buffer].bytes;
	bytes.resize(static_cast<size_t>(size));
	if (data != nullptr && size > 0) {
		std::memcpy(bytes.data(), data, static_cast<size_t>(size));
	}
}

GLenum glCheckFramebufferStatus(GLenum target) {
	if (target != GL_FRAMEBUFFER) {
		set_error(GL_INVALID_ENUM);
		return 0;
	}
	if (bound_framebuffer == 0 || framebuffers[bound_framebuffer].colour_texture != 0) {
		return GL_FRAMEBUFFER_COMPLETE;
	}
	return 0;
}

void glClear(GLbitfield) {
	/* Clear state is accepted; VirGL emission belongs to the frame encoder. */
}

void glCompileShader(GLuint shader) {
	auto found = shaders.find(shader);
	if (found == shaders.end()) {
		set_error(GL_INVALID_VALUE);
		return;
	}
#ifdef WL_AMIGAOS4_VIRTIO_NO_SHADERS
	found->second.compiled = !found->second.source.empty();
	found->second.log = found->second.compiled ? "" : "No shader source was supplied";
#else
	found->second.compiled = false;
	found->second.log = "AmigaOS4 VirtIO: GLSL-to-VirGL translation is not implemented yet";
#endif
}

GLuint glCreateProgram(void) {
	const GLuint id = allocate_object();
	programs.emplace(id, ProgramState{});
	return id;
}

GLuint glCreateShader(GLenum type) {
	if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) {
		set_error(GL_INVALID_ENUM);
		return 0;
	}
	const GLuint id = allocate_object();
	ShaderState state;
	state.type = type;
	shaders.emplace(id, std::move(state));
	return id;
}

void glDeleteBuffers(GLsizei count, const GLuint* objects) {
	delete_objects(count, objects, &buffers);
}
void glDeleteFramebuffers(GLsizei count, const GLuint* objects) {
	delete_objects(count, objects, &framebuffers);
}
void glDeleteProgram(GLuint program) {
	programs.erase(program);
}
void glDeleteShader(GLuint shader) {
	shaders.erase(shader);
}
void glDeleteTextures(GLsizei count, const GLuint* objects) {
	delete_objects(count, objects, &textures);
}

void glDepthFunc(GLenum) {
}
void glDisable(GLenum) {
}
void glDisableVertexAttribArray(GLuint) {
}

void glDrawArrays(GLenum mode, GLint, GLsizei count) {
	if ((mode != GL_TRIANGLES && mode != GL_LINES) || count < 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (current_program == 0 || programs.count(current_program) == 0 ||
	    !programs[current_program].linked || bound_buffer == 0) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	/* Diagnostic no-shader mode intentionally discards valid draws.  Clears,
	 * event processing and direct VirtIO presentation still exercise the full
	 * application/window/backend lifecycle. */
}

void glDrawBuffer(GLenum buffer) {
	if (buffer != GL_BACK && buffer != GL_COLOR_ATTACHMENT0) {
		set_error(GL_INVALID_ENUM);
	}
}
void glEnable(GLenum) {
}
void glEnableVertexAttribArray(GLuint) {
}
void glFlush(void) {
}

void glFramebufferTexture2D(
   GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
	if (target != GL_FRAMEBUFFER || attachment != GL_COLOR_ATTACHMENT0 ||
	    textarget != GL_TEXTURE_2D || level != 0 || bound_framebuffer == 0 ||
	    textures.count(texture) == 0) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	framebuffers[bound_framebuffer].colour_texture = texture;
}

void glGenBuffers(GLsizei count, GLuint* objects) {
	generate_objects(count, objects, &buffers);
}
void glGenFramebuffers(GLsizei count, GLuint* objects) {
	generate_objects(count, objects, &framebuffers);
}
void glGenTextures(GLsizei count, GLuint* objects) {
	generate_objects(count, objects, &textures);
}

GLint glGetAttribLocation(GLuint program, const GLchar* name) {
	if (programs.count(program) == 0 || !programs[program].linked) {
		set_error(GL_INVALID_OPERATION);
		return -1;
	}
	if (name == nullptr) {
		set_error(GL_INVALID_VALUE);
		return -1;
	}
	auto& locations = programs[program].attributes;
	const auto found = locations.find(name);
	if (found != locations.end()) {
		return found->second;
	}
	const GLint location = static_cast<GLint>(locations.size());
	locations.emplace(name, location);
	return location;
}

void glGetBooleanv(GLenum name, GLboolean* value) {
	if (value == nullptr) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (name == GL_DOUBLEBUFFER) {
		*value = GL_TRUE;
	} else {
		set_error(GL_INVALID_ENUM);
		*value = GL_FALSE;
	}
}

GLenum glGetError(void) {
	const GLenum result = pending_error;
	pending_error = GL_NO_ERROR;
	return result;
}

void glGetIntegerv(GLenum name, GLint* value) {
	if (value == nullptr) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (name == GL_MAX_TEXTURE_SIZE) {
		*value = 8192;
	} else {
		set_error(GL_INVALID_ENUM);
		*value = 0;
	}
}

void glGetProgramInfoLog(GLuint program, GLsizei capacity, GLsizei* length, GLchar* output) {
	if (programs.count(program) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	copy_log(programs[program].log, capacity, length, output);
}

void glGetProgramiv(GLuint program, GLenum name, GLint* value) {
	if (programs.count(program) == 0 || value == nullptr) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (name == GL_LINK_STATUS) {
		*value = programs[program].linked ? GL_TRUE : GL_FALSE;
	} else if (name == GL_INFO_LOG_LENGTH) {
		*value = static_cast<GLint>(programs[program].log.size() + 1);
	} else {
		set_error(GL_INVALID_ENUM);
	}
}

void glGetShaderInfoLog(GLuint shader, GLsizei capacity, GLsizei* length, GLchar* output) {
	if (shaders.count(shader) == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	copy_log(shaders[shader].log, capacity, length, output);
}

void glGetShaderiv(GLuint shader, GLenum name, GLint* value) {
	if (shaders.count(shader) == 0 || value == nullptr) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (name == GL_COMPILE_STATUS) {
		*value = shaders[shader].compiled ? GL_TRUE : GL_FALSE;
	} else if (name == GL_INFO_LOG_LENGTH) {
		*value = static_cast<GLint>(shaders[shader].log.size() + 1);
	} else {
		set_error(GL_INVALID_ENUM);
	}
}

const GLubyte* glGetString(GLenum name) {
#ifdef WL_AMIGAOS4_VIRTIO_NO_SHADERS
	static const GLubyte version[] = "2.1 Widelands VirtIO/VirGL no-shader test";
	static const GLubyte shading[] = "1.20 dummy no-shader diagnostic";
#else
	static const GLubyte version[] = "2.1 Widelands VirtIO/VirGL";
	static const GLubyte shading[] = "1.20 Widelands VirtIO frontend";
#endif
	if (name == GL_VERSION) {
		return version;
	}
	if (name == GL_SHADING_LANGUAGE_VERSION) {
		return shading;
	}
	set_error(GL_INVALID_ENUM);
	return nullptr;
}

void glGetTexImage(GLenum, GLint, GLenum, GLenum, void*) {
	set_error(GL_INVALID_OPERATION);
}

GLint glGetUniformLocation(GLuint program, const GLchar* name) {
	if (programs.count(program) == 0 || !programs[program].linked) {
		set_error(GL_INVALID_OPERATION);
		return -1;
	}
	if (name == nullptr) {
		set_error(GL_INVALID_VALUE);
		return -1;
	}
	auto& locations = programs[program].uniforms;
	const auto found = locations.find(name);
	if (found != locations.end()) {
		return found->second;
	}
	const GLint location = static_cast<GLint>(locations.size());
	locations.emplace(name, location);
	return location;
}

void glLinkProgram(GLuint program) {
	auto found = programs.find(program);
	if (found == programs.end()) {
		set_error(GL_INVALID_VALUE);
		return;
	}
#ifdef WL_AMIGAOS4_VIRTIO_NO_SHADERS
	found->second.linked = !found->second.shaders.empty();
	for (const GLuint shader : found->second.shaders) {
		found->second.linked = found->second.linked && shaders.count(shader) != 0 &&
		                       shaders[shader].compiled;
	}
	found->second.log = found->second.linked ? "" : "Dummy program has an uncompiled shader";
#else
	found->second.linked = false;
	found->second.log = "AmigaOS4 VirtIO: shader program translation is not implemented yet";
#endif
}

void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) {
	set_error(GL_INVALID_OPERATION);
}
void glScissor(GLint, GLint, GLsizei, GLsizei) {
}

void glShaderSource(
   GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths) {
	auto found = shaders.find(shader);
	if (found == shaders.end() || count < 0 || (count > 0 && strings == nullptr)) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	found->second.source.clear();
	for (GLsizei i = 0; i < count; ++i) {
		if (strings[i] == nullptr) {
			continue;
		}
		if (lengths != nullptr && lengths[i] >= 0) {
			found->second.source.append(strings[i], static_cast<size_t>(lengths[i]));
		} else {
			found->second.source.append(strings[i]);
		}
	}
}

void glTexImage2D(GLenum target,
                  GLint level,
                  GLint internal_format,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) {
	const GLuint texture = bound_textures[active_texture_unit];
	if (target != GL_TEXTURE_2D || level != 0 || border != 0 || format != GL_RGBA ||
	    type != GL_UNSIGNED_BYTE || width < 0 || height < 0 || texture == 0) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	auto& state = textures[texture];
	state.width = width;
	state.height = height;
	state.internal_format = internal_format;
	#ifdef WL_AMIGAOS4_VIRTIO_NO_SHADERS
	/* Draws are discarded in this diagnostic mode. Keeping a CPU copy of every
	 * decoded image exhausts AmigaOS memory while Widelands builds its texture
	 * atlases, so retain only the object and dimension metadata. */
	state.pixels.clear();
	#else
	const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	state.pixels.resize(size);
	if (pixels != nullptr && size > 0) {
		std::memcpy(state.pixels.data(), pixels, size);
	}
	#endif
}

void glTexParameteri(GLenum target, GLenum, GLint) {
	if (target != GL_TEXTURE_2D || bound_textures[active_texture_unit] == 0) {
		set_error(GL_INVALID_OPERATION);
	}
}

void glUniform1f(GLint location, GLfloat) {
	if (location < 0) {
		return;
	}
	if (current_program == 0) {
		set_error(GL_INVALID_OPERATION);
	}
}
void glUniform1i(GLint location, GLint) {
	if (location < 0) {
		return;
	}
	if (current_program == 0) {
		set_error(GL_INVALID_OPERATION);
	}
}
void glUniform2f(GLint location, GLfloat, GLfloat) {
	if (location < 0) {
		return;
	}
	if (current_program == 0) {
		set_error(GL_INVALID_OPERATION);
	}
}

void glUseProgram(GLuint program) {
	if (program != 0 && (programs.count(program) == 0 || !programs[program].linked)) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	current_program = program;
}

void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {
	if (bound_buffer == 0) {
		set_error(GL_INVALID_OPERATION);
	}
}
void glViewport(GLint, GLint, GLsizei width, GLsizei height) {
	if (width < 0 || height < 0) {
		set_error(GL_INVALID_VALUE);
	}
}

}  // extern "C"

#endif  // WL_AMIGAOS4_VIRTIO_GL
