/*
 * Copyright (C) 2026 by the Widelands Development Team
 *
 * First-stage OpenGL 2.1 state frontend for the AmigaOS4 VirtIO backend.
 * This translation unit deliberately owns the GL ABI so the direct backend
 * never falls through to gl4es/MiniGL.  Unsupported rendering operations fail
 * honestly until their VirGL translations are implemented.
 */

#include "graphic/virtio/gl_api.h"
#include "graphic/virtio/virtgl_bridge.h"

#include "base/log.h"

#ifdef WL_AMIGAOS4_VIRTIO_GL

#include <algorithm>
#include <cmath>
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

/* What a vertex attribute means.
 *
 * Widelands names its attributes, and the names are unambiguous across all
 * eight of its programs -- there are only ten of them in total. That is what
 * makes the translation possible without touching a shader: the name says
 * whether a float pair is a position, a texture coordinate or an atlas
 * offset, and the fixed-function layer below has a place for each. */
enum class AttrKind {
	kUnknown,
	kPosition,
	kTexturePosition,
	kTextureOffset,
	kMaskTexturePosition,
	kDitherTexturePosition,
	kBrightness,
	kColor,
	kBlend,
	kOverlay,
	kProgramFlavor
};

AttrKind attr_kind_of(const std::string& name) {
	if (name == "attr_position") { return AttrKind::kPosition; }
	if (name == "attr_texture_position") { return AttrKind::kTexturePosition; }
	if (name == "attr_texture_offset") { return AttrKind::kTextureOffset; }
	if (name == "attr_mask_texture_position") { return AttrKind::kMaskTexturePosition; }
	if (name == "attr_dither_texture_position") { return AttrKind::kDitherTexturePosition; }
	if (name == "attr_brightness") { return AttrKind::kBrightness; }
	if (name == "attr_color") { return AttrKind::kColor; }
	if (name == "attr_blend") { return AttrKind::kBlend; }
	if (name == "attr_overlay") { return AttrKind::kOverlay; }
	if (name == "attr_program_flavor") { return AttrKind::kProgramFlavor; }
	return AttrKind::kUnknown;
}

/* Which of the eight programs this is.
 *
 * The attribute names say what each float means, but not what the fragment
 * shader does with it, and that differs enough between the eight that one
 * rule cannot serve them all. A blit's blend colour, for instance, is
 * RGBAColor(0, 0, 0, opacity): its rgb is deliberately black and the shader
 * uses only the alpha. Modulating by it -- the obvious reading of "texture
 * times colour" -- turns the whole screen black, which is exactly what it
 * did.
 *
 * The shader source is already here, so each program can simply say which
 * one it is, once, at link time. */
enum class ProgramKind {
	kUnknown,
	kBlit,
	kFillRect,
	kDrawLine,
	kGrid,
	kWorkarea,
	kRoad,
	kTerrain,
	kDither
};

ProgramKind program_kind_of(const std::string& source) {
	if (source.find("out_program_flavor") != std::string::npos) { return ProgramKind::kBlit; }
	if (source.find("u_dither_texture") != std::string::npos) { return ProgramKind::kDither; }
	if (source.find("u_terrain_texture") != std::string::npos) { return ProgramKind::kTerrain; }
	if (source.find("var_overlay") != std::string::npos) { return ProgramKind::kWorkarea; }
	if (source.find("out_brightness") != std::string::npos) { return ProgramKind::kRoad; }
	if (source.find("pow(cos(") != std::string::npos) { return ProgramKind::kDrawLine; }
	if (source.find("vec4(var_color, .8)") != std::string::npos) { return ProgramKind::kGrid; }
	if (source.find("var_color") != std::string::npos) { return ProgramKind::kFillRect; }
	return ProgramKind::kUnknown;
}

struct ProgramState {
	std::set<GLuint> shaders;
	std::string log;
	bool linked{false};
	std::map<std::string, GLint> attributes;
	std::map<std::string, GLint> uniforms;
	/* Filled in as locations are handed out, so a draw can ask what a
	   location means without searching the map by value. */
	std::map<GLint, AttrKind> attribute_kind;
	std::map<GLint, std::string> uniform_name;
	ProgramKind kind{ProgramKind::kUnknown};
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

struct AttribArray {
	bool enabled{false};
	GLint size{0};
	GLenum type{0};
	GLsizei stride{0};
	std::size_t offset{0};
	GLuint buffer{0};
};

constexpr unsigned kMaxAttribs = 16;
AttribArray attribs[kMaxAttribs];

/* The uniforms that carry geometry rather than a sampler unit. u_z_value is
   the clip-space z every vertex shader writes; u_texture_dimensions scales
   the atlas coordinate the terrain and dither programs compute. */
float uniform_z_value = 0.0f;
float uniform_texture_dimensions[2] = {1.0f, 1.0f};

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
	wlgl_glActiveTextureARB(texture);
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
	wlgl_glBindTexture(target, texture);
}

void glBlendEquation(GLenum) {
}
void glBlendFunc(GLenum source, GLenum destination) {
	wlgl_glBlendFunc(source, destination);
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

void glClear(GLbitfield mask) {
	wlgl_glClear(mask);
}

void glCompileShader(GLuint shader) {
	auto found = shaders.find(shader);
	if (found == shaders.end()) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	/* The source is accepted and then ignored, and that is not a stub.
	 *
	 * Nothing below this reads a shader: glDrawArrays walks the vertex array the
	 * program uploaded and identifies what each attribute means from its name,
	 * because Widelands' eight fragment shaders come down to texture times
	 * colour -- which the fixed-function layer does natively. Failing the
	 * compile is what stopped the game at the dither mask, since RenderQueue
	 * builds all eight programs before anything is drawn. */
	found->second.compiled = !found->second.source.empty();
	found->second.log = found->second.compiled ? "" : "No shader source was supplied";
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
	wlgl_glDeleteTextures(count, objects);
	delete_objects(count, objects, &textures);
}

void glDepthFunc(GLenum func) {
	wlgl_glDepthFunc(func);
}
void glDisable(GLenum capability) {
	wlgl_glDisable(capability);
}
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean,
                           GLsizei stride, const void* pointer) {
	if (index >= kMaxAttribs || size < 1 || size > 4) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	AttribArray& a = attribs[index];
	a.size = size;
	a.type = type;
	a.stride = stride;
	/* A byte offset into whatever buffer is bound now, which is how a
	   buffer-backed attribute is specified. */
	a.offset = reinterpret_cast<std::size_t>(pointer);
	a.buffer = bound_buffer;
}

void glDisableVertexAttribArray(GLuint index) {
	if (index < kMaxAttribs) {
		attribs[index].enabled = false;
	}
}


/* Reads one attribute of one vertex out of the bound buffer.
 *
 * Widelands uploads its vertices with glBufferData and points the attributes
 * at byte offsets inside that buffer, so the data is here and the descriptors
 * say how to walk it. Everything it sends is float; anything else would be a
 * program this translation has not seen. */
bool read_attrib(const AttribArray& a, GLsizei vertex, float* out, int wanted) {
	if (!a.enabled || a.type != GL_FLOAT || a.buffer == 0) {
		return false;
	}
	const auto found = buffers.find(a.buffer);
	if (found == buffers.end()) {
		return false;
	}
	const std::vector<unsigned char>& bytes = found->second.bytes;
	const std::size_t stride =
	   a.stride > 0 ? static_cast<std::size_t>(a.stride)
	                : static_cast<std::size_t>(a.size) * sizeof(float);
	const std::size_t base = a.offset + stride * static_cast<std::size_t>(vertex);
	const int count = a.size < wanted ? a.size : wanted;
	if (base + static_cast<std::size_t>(count) * sizeof(float) > bytes.size()) {
		return false;
	}
	for (int i = 0; i < count; ++i) {
		float value;
		std::memcpy(&value, &bytes[base + static_cast<std::size_t>(i) * sizeof(float)],
		            sizeof(float));
		out[i] = value;
	}
	return true;
}

/* The translation itself.
 *
 * Widelands' vertex shaders pass their attributes through untouched -- every
 * one of them writes gl_Position = vec4(attr_position, u_z_value, 1.) -- and
 * its fragment shaders come down to texture times colour, which is what the
 * fixed-function layer does natively. So there is nothing to compile: walking
 * the array and feeding the layer one vertex at a time produces the same
 * image the shader would have.
 *
 * The two exceptions are terrain and dither, whose fragment shaders index a
 * texture atlas with a per-fragment fract(). That is computed here per vertex
 * instead. It agrees with the shader wherever a triangle stays inside one
 * tile, which is how Widelands lays its terrain out, and differs only where
 * one spans a tile boundary. */
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
	/* Every refusal below used to be silent, which is why the log could show a
	   game presenting six hundred frames with not one triangle in them. Each
	   reason reports once, and says which one it was. */
	static bool told_mode = false, told_state = false, told_position = false;
	if ((mode != GL_TRIANGLES && mode != GL_LINES) || count < 0) {
		if (!told_mode) {
			told_mode = true;
			log_info("VirtIO GL: draw REFUSED, mode 0x%x count %d", mode, count);
		}
		set_error(GL_INVALID_VALUE);
		return;
	}
	if (current_program == 0 || programs.count(current_program) == 0 ||
	    !programs[current_program].linked || bound_buffer == 0) {
		if (!told_state) {
			told_state = true;
			log_info("VirtIO GL: draw REFUSED, program %u known=%d linked=%d buffer %u",
			         current_program, static_cast<int>(programs.count(current_program) != 0),
			         static_cast<int>(programs.count(current_program) != 0 &&
			                          programs[current_program].linked),
			         bound_buffer);
		}
		set_error(GL_INVALID_OPERATION);
		return;
	}

	const ProgramState& program = programs[current_program];

	/* Which location carries what, looked up once for the whole draw. */
	const AttribArray* position = nullptr;
	const AttribArray* texture_position = nullptr;
	const AttribArray* texture_offset = nullptr;
	const AttribArray* second_texture = nullptr;
	const AttribArray* brightness = nullptr;
	const AttribArray* colour = nullptr;
	const AttribArray* flavour = nullptr;
	for (unsigned i = 0; i < kMaxAttribs; ++i) {
		if (!attribs[i].enabled) {
			continue;
		}
		const auto kind = program.attribute_kind.find(static_cast<GLint>(i));
		if (kind == program.attribute_kind.end()) {
			continue;
		}
		switch (kind->second) {
		case AttrKind::kPosition: position = &attribs[i]; break;
		case AttrKind::kTexturePosition: texture_position = &attribs[i]; break;
		case AttrKind::kTextureOffset: texture_offset = &attribs[i]; break;
		case AttrKind::kMaskTexturePosition:
		case AttrKind::kDitherTexturePosition: second_texture = &attribs[i]; break;
		case AttrKind::kBrightness: brightness = &attribs[i]; break;
		case AttrKind::kColor:
		case AttrKind::kBlend:
		case AttrKind::kOverlay: colour = &attribs[i]; break;
		case AttrKind::kProgramFlavor: flavour = &attribs[i]; break;
		default: break;
		}
	}
	if (position == nullptr) {
		if (!told_position) {
			told_position = true;
			unsigned enabled = 0, mapped = 0;
			for (unsigned i = 0; i < kMaxAttribs; ++i) {
				if (attribs[i].enabled) {
					++enabled;
					if (program.attribute_kind.count(static_cast<GLint>(i)) != 0) {
						++mapped;
					}
				}
			}
			log_info("VirtIO GL: draw REFUSED, no position: program %u, %u enabled arrays, "
			         "%u of them named, %u names known",
			         current_program, enabled, mapped,
			         static_cast<unsigned>(program.attribute_kind.size()));
		}
		set_error(GL_INVALID_OPERATION);
		return;
	}

	static unsigned draws = 0;
	if (draws < 6) {
		/* Which attributes were recognised, so a draw that produces nothing
		   visible can be told from one that was never given a texture
		   coordinate or a colour in the first place. */
		log_info("VirtIO GL: draw %u, %d vertices, program %u, tex=%d off=%d tex2=%d "
		         "bright=%d colour=%d z=%.3f dim=%.3f,%.3f",
		         draws, count, current_program, static_cast<int>(texture_position != nullptr),
		         static_cast<int>(texture_offset != nullptr),
		         static_cast<int>(second_texture != nullptr),
		         static_cast<int>(brightness != nullptr), static_cast<int>(colour != nullptr),
		         uniform_z_value, uniform_texture_dimensions[0], uniform_texture_dimensions[1]);
	}
	++draws;
	/* Widelands drives a programmable pipeline, so it never enables
	   GL_TEXTURE_2D -- a shader samples whatever it is given. The fixed
	   function layer below only samples an enabled unit, so the draw has to
	   say so itself, and say it per draw: a blit with no mask binds nothing
	   to unit 1, and leaving that unit enabled would modulate the result with
	   a texture that is not there. */
	const bool want_unit0 = texture_position != nullptr && bound_textures[0] != 0;
	/* Unit 1 stays off. Both shaders that bind a second texture read it as
	   something other than a colour to multiply by -- blit takes a player
	   colour mask from it, dither an alpha -- so modulating with it would be
	   wrong in a way that a missing second texture is not. */
	const bool want_unit1 = false;
	/* Only when it changes. This used to issue four calls per draw whether or
	   not anything differed -- around 1750 per frame on a screen of 438
	   batches -- and state the layer has to act on is state that can stop two
	   batches merging. The layer keeps this per unit and nothing else here
	   touches GL_TEXTURE_2D (Widelands drives a programmable pipeline and
	   never enables it), so remembering what was set is enough. */
	static bool unit_enabled[2] = {false, false};
	static bool unit_state_known = false;
	if (!unit_state_known) {
		/* Unit 1 goes off once and stays off: both shaders that bind a
		   second texture read it as something other than a colour to
		   multiply by. */
		unit_state_known = true;
		wlgl_glActiveTextureARB(GL_TEXTURE1);
		wlgl_glDisable(GL_TEXTURE_2D);
		unit_enabled[1] = false;
		wlgl_glActiveTextureARB(GL_TEXTURE0);
		active_texture_unit = 0;
	}
	static_cast<void>(want_unit1);
	if (unit_enabled[0] != want_unit0) {
		if (active_texture_unit != 0) {
			wlgl_glActiveTextureARB(GL_TEXTURE0);
			active_texture_unit = 0;
		}
		if (want_unit0) {
			wlgl_glEnable(GL_TEXTURE_2D);
		} else {
			wlgl_glDisable(GL_TEXTURE_2D);
		}
		unit_enabled[0] = want_unit0;
	}

	wlgl_glBegin(mode == GL_LINES ? GL_LINES : GL_TRIANGLES);
	for (GLsizei index = 0; index < count; ++index) {
		const GLsizei vertex = first + index;

		/* What the fragment shader would have produced, expressed as the
		   vertex colour a GL_MODULATE unit needs to produce the same thing. */
		float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		switch (program.kind) {
		case ProgramKind::kBlit: {
			/* vec4(texture.rgb, blend.a * texture.a) for the plain flavour:
			   the blend colour carries opacity and nothing else. Modulating
			   by its rgb -- which Widelands sets to black on purpose -- is
			   what made every blit come out black. */
			float blend[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			if (colour != nullptr) {
				read_attrib(*colour, vertex, blend, 4);
			}
			float which = 0.0f;
			if (flavour != nullptr) {
				read_attrib(*flavour, vertex, &which, 1);
			}
			if (which > 0.5f && which < 1.5f) {
				/* Monochrome: luminance(texture) * blend.rgb. The luminance is
				   per fragment and out of reach here, so the tint is applied to
				   the texture instead -- the right colour, not yet grey. */
				rgba[0] = blend[0];
				rgba[1] = blend[1];
				rgba[2] = blend[2];
			}
			/* Flavour 2 mixes in a player colour through a mask, which is also
			   per fragment. Drawn as the plain flavour until the layer can do
			   it: the image is right, the player colour is missing. */
			rgba[3] = blend[3];
		} break;

		case ProgramKind::kGrid:
			/* vec4(var_color, .8) -- the alpha is a constant in the shader,
			   and the attribute only has three components. */
			if (colour != nullptr) {
				read_attrib(*colour, vertex, rgba, 3);
			}
			rgba[3] = 0.8f;
			break;

		case ProgramKind::kDrawLine:
			/* The alpha attribute is a distance across the line, shaped by
			   pow(cos(a * PI/2), 1.5) into the soft edge. Per vertex is exact
			   here: it is a function of the attribute, not of the fragment. */
			if (colour != nullptr) {
				read_attrib(*colour, vertex, rgba, 4);
				rgba[3] = std::pow(std::cos(rgba[3] * 3.14159265f / 2.0f), 1.5f);
			}
			break;

		default:
			if (colour != nullptr) {
				read_attrib(*colour, vertex, rgba, 4);
				if (colour->size < 4) {
					rgba[3] = 1.0f;
				}
			}
			break;
		}
		if (brightness != nullptr) {
			float value = 1.0f;
			if (read_attrib(*brightness, vertex, &value, 1)) {
				/* clr.rgb *= var_brightness, which is exactly what the layer
				   does with a vertex colour under GL_MODULATE. */
				rgba[0] *= value;
				rgba[1] *= value;
				rgba[2] *= value;
			}
		}
		wlgl_glColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);

		float uv_logged[2] = {0.0f, 0.0f};
		if (texture_position != nullptr) {
			float uv[2] = {0.0f, 0.0f};
			read_attrib(*texture_position, vertex, uv, 2);
			if (texture_offset != nullptr) {
				/* The atlas: fract() clamped inside a margin, then scaled by
				   the tile's size and moved to its corner. The margin is the
				   shader's own, and it exists so a sample never lands on a
				   neighbouring tile. */
				const float margin = 1e-2f;
				float offset[2] = {0.0f, 0.0f};
				read_attrib(*texture_offset, vertex, offset, 2);
				for (int c = 0; c < 2; ++c) {
					float f = uv[c] - std::floor(uv[c]);
					if (f < margin) { f = margin; }
					if (f > 1.0f - margin) { f = 1.0f - margin; }
					uv[c] = offset[c] + uniform_texture_dimensions[c] * f;
				}
			}
			wlgl_glTexCoord2f(uv[0], uv[1]);
			uv_logged[0] = uv[0];
			uv_logged[1] = uv[1];
		}
		if (second_texture != nullptr) {
			float uv[2] = {0.0f, 0.0f};
			read_attrib(*second_texture, vertex, uv, 2);
			wlgl_glMultiTexCoord2fARB(GL_TEXTURE1, uv[0], uv[1]);
		}

		float xyz[3] = {0.0f, 0.0f, uniform_z_value};
		read_attrib(*position, vertex, xyz, 3);
		if (position->size < 3) {
			xyz[2] = uniform_z_value;
		}
		/* The first corners of the first few draws, because a draw that
		   reports the right attributes can still be reading the wrong bytes:
		   a wrong stride or offset produces coordinates far outside the
		   clip cube and nothing on screen, and looks identical from here. */
		if (draws < 3 && index < 3) {
			log_info("VirtIO GL:   v%d xyz %.3f %.3f %.3f  uv %.3f %.3f  rgba %.2f %.2f %.2f %.2f",
			         static_cast<int>(index), xyz[0], xyz[1], xyz[2],
			         texture_position != nullptr ? uv_logged[0] : 0.0f,
			         texture_position != nullptr ? uv_logged[1] : 0.0f, rgba[0], rgba[1], rgba[2],
			         rgba[3]);
		}
		wlgl_glVertex3f(xyz[0], xyz[1], xyz[2]);
	}
	wlgl_glEnd();
}

void glDrawBuffer(GLenum buffer) {
	if (buffer != GL_BACK && buffer != GL_COLOR_ATTACHMENT0) {
		set_error(GL_INVALID_ENUM);
	}
}
void glEnable(GLenum capability) {
	wlgl_glEnable(capability);
}
void glEnableVertexAttribArray(GLuint index) {
	if (index < kMaxAttribs) {
		attribs[index].enabled = true;
	}
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
	/* The layer hands out the names, so both sides agree on them and a
	   texture the frontend records is the same one the layer uploads to. */
	wlgl_glGenTextures(count, objects);
	for (GLsizei i = 0; i < count; ++i) {
		textures[objects[i]] = TextureState();
	}
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
	/* This is the whole translation: the location Widelands is about to bind
	   its vertex array to, recorded against what the attribute means. Without
	   it a draw has an array of floats and no idea which of them is a
	   position, and glDrawArrays turns every one of them away. */
	programs[program].attribute_kind.emplace(location, attr_kind_of(name));
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
		/* Asked of the layer rather than claimed here. It caps textures at its
		   own maximum and *downscales* anything larger without a word, so a
		   number invented at this end would have Widelands build atlases eight
		   times too big and every coordinate in them wrong -- silently, which is
		   the worst kind. */
		wlgl_glGetIntegerv(GL_MAX_TEXTURE_SIZE, value);
		if (*value <= 0) {
			*value = 1024;
		}
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
	/* glUniform1f and glUniform2f look a location back up here to recognise
	   u_z_value and u_texture_dimensions. The name was being recorded by
	   glGetAttribLocation instead -- a different, overlapping numbering -- so
	   the two uniforms that matter were never seen and every vertex got the
	   default z and an unscaled atlas coordinate. */
	programs[program].uniform_name.emplace(location, name);
	return location;
}

void glLinkProgram(GLuint program) {
	auto found = programs.find(program);
	if (found == programs.end()) {
		set_error(GL_INVALID_VALUE);
		return;
	}
	found->second.linked = !found->second.shaders.empty();
	for (const GLuint shader : found->second.shaders) {
		found->second.linked = found->second.linked && shaders.count(shader) != 0 &&
		                       shaders[shader].compiled;
	}
	found->second.log = found->second.linked ? "" : "Program has an uncompiled shader";
	if (found->second.linked) {
		/* Only the fragment shader distinguishes them: all eight vertex
		   shaders do little more than pass their attributes through. */
		for (const GLuint shader : found->second.shaders) {
			if (shaders[shader].type != GL_FRAGMENT_SHADER) {
				continue;
			}
			found->second.kind = program_kind_of(shaders[shader].source);
			log_info("VirtIO GL: program %u is kind %d", program,
			         static_cast<int>(found->second.kind));
		}
	}
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
	/* The first few uploads, because this is the first place a real texture
	   reaches the layer and a black screen with sound is what it looks like
	   when one of them does not come back. */
	static unsigned uploads = 0;
	if (uploads < 4) {
		log_info("VirtIO GL: texture %u upload %dx%d fmt=0x%x", texture, width,
		         height, format);
	}
	++uploads;
	/* No CPU copy is kept. The pixels go straight to the layer, which owns
	   them from here; holding a second copy of every decoded image is what
	   exhausted memory while Widelands built its texture atlases. Only the
	   size stays behind, for the queries Widelands makes about it. */
	wlgl_glTexImage2D(target, level, internal_format, width, height, border,
	                  format, type, pixels);
	state.pixels.clear();
}

void glTexParameteri(GLenum target, GLenum name, GLint value) {
	if (target != GL_TEXTURE_2D || bound_textures[active_texture_unit] == 0) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	wlgl_glTexParameteri(target, name, value);
}

void glUniform1f(GLint location, GLfloat value) {
	if (location < 0) {
		return;
	}
	if (current_program == 0) {
		set_error(GL_INVALID_OPERATION);
		return;
	}
	const auto& names = programs[current_program].uniform_name;
	const auto found = names.find(location);
	if (found != names.end() && found->second == "u_z_value") {
		uniform_z_value = value;
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
void glUniform2f(GLint location, GLfloat x, GLfloat y) {
	if (location < 0) {
		return;
	}
	if (current_program != 0) {
		const auto& names = programs[current_program].uniform_name;
		const auto found = names.find(location);
		if (found != names.end() && found->second == "u_texture_dimensions") {
			uniform_texture_dimensions[0] = x;
			uniform_texture_dimensions[1] = y;
		}
	}
	if (false) {
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

void glViewport(GLint, GLint, GLsizei width, GLsizei height) {
	if (width < 0 || height < 0) {
		set_error(GL_INVALID_VALUE);
	}
}

}  // extern "C"

#endif  // WL_AMIGAOS4_VIRTIO_GL
