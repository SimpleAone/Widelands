#include "graphic/virtio/virgl_encoder.h"

#include <stddef.h>
#include <string.h>

enum {
    VIRGL_CREATE_OBJECT = 1,
    VIRGL_BIND_OBJECT = 2,
    VIRGL_SET_VIEWPORT = 4,
    VIRGL_SET_FRAMEBUFFER = 5,
    VIRGL_SET_VERTEX_BUFFERS = 6,
    VIRGL_CLEAR = 7,
    VIRGL_DRAW_VBO = 8,
    VIRGL_RESOURCE_INLINE_WRITE = 9,
    VIRGL_SET_SAMPLER_VIEWS = 10,
    VIRGL_SET_CONSTANT_BUFFER = 12,
    VIRGL_BIND_SAMPLER_STATES = 18,
    VIRGL_BIND_SHADER = 31,
    VIRGL_OBJECT_BLEND = 1,
    VIRGL_OBJECT_RASTERIZER = 2,
    VIRGL_OBJECT_DSA = 3,
    VIRGL_OBJECT_SHADER = 4,
    VIRGL_OBJECT_VERTEX_ELEMENTS = 5,
    VIRGL_OBJECT_SAMPLER_VIEW = 6,
    VIRGL_OBJECT_SAMPLER_STATE = 7,
    VIRGL_OBJECT_SURFACE = 8
};

static int reserve(vgpu_virgl_encoder *encoder, uint32_t length) {
    if (!encoder || encoder->failed || length > encoder->capacity - encoder->length) {
        if (encoder) encoder->failed = 1;
        return 0;
    }
    return 1;
}

void vgpu_virgl_encoder_init(vgpu_virgl_encoder *encoder, void *buffer,
                             uint32_t capacity) {
    if (!encoder) return;
    encoder->bytes = (uint8_t *)buffer;
    encoder->capacity = capacity;
    encoder->length = 0U;
    encoder->failed = buffer == NULL;
}

int vgpu_virgl_encoder_ok(const vgpu_virgl_encoder *encoder) {
    return encoder && !encoder->failed && (encoder->length & 3U) == 0U;
}

uint32_t vgpu_virgl_encoder_size(const vgpu_virgl_encoder *encoder) {
    return encoder ? encoder->length : 0U;
}

void vgpu_virgl_dword(vgpu_virgl_encoder *encoder, uint32_t value) {
    uint8_t *out;
    if (!reserve(encoder, 4U)) return;
    out = encoder->bytes + encoder->length;
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
    encoder->length += 4U;
}

void vgpu_virgl_bytes(vgpu_virgl_encoder *encoder, const void *bytes,
                      uint32_t length) {
    uint32_t padded = (length + 3U) & ~3U;
    if (!bytes || !reserve(encoder, padded)) return;
    (void)memcpy(encoder->bytes + encoder->length, bytes, length);
    (void)memset(encoder->bytes + encoder->length + length, 0,
                 padded - length);
    encoder->length += padded;
}

void vgpu_virgl_command(vgpu_virgl_encoder *encoder, uint8_t command,
                        uint8_t object, uint16_t payload_dwords) {
    vgpu_virgl_dword(encoder, (uint32_t)command | ((uint32_t)object << 8) |
                              ((uint32_t)payload_dwords << 16));
}

void vgpu_virgl_bind_object(vgpu_virgl_encoder *encoder, uint8_t object,
                            uint32_t handle) {
    vgpu_virgl_command(encoder, VIRGL_BIND_OBJECT, object, 1U);
    vgpu_virgl_dword(encoder, handle);
}

void vgpu_virgl_create_surface(vgpu_virgl_encoder *encoder, uint32_t handle,
                               uint32_t resource, uint32_t format) {
    vgpu_virgl_create_surface_layer(encoder, handle, resource, format, 0U);
}

void vgpu_virgl_create_surface_layer(vgpu_virgl_encoder *encoder,
                                     uint32_t handle, uint32_t resource,
                                     uint32_t format, uint32_t layer) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, format);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, layer | (layer << 16U));
}

void vgpu_virgl_set_framebuffer(vgpu_virgl_encoder *encoder,
                                uint32_t colour_surface) {
    vgpu_virgl_set_framebuffer_depth(encoder, colour_surface, 0U);
}

void vgpu_virgl_set_framebuffer_depth(vgpu_virgl_encoder *encoder,
                                      uint32_t colour_surface,
                                      uint32_t depth_surface) {
    vgpu_virgl_command(encoder, VIRGL_SET_FRAMEBUFFER, 0U, 3U);
    vgpu_virgl_dword(encoder, 1U);
    vgpu_virgl_dword(encoder, depth_surface);
    vgpu_virgl_dword(encoder, colour_surface);
}

void vgpu_virgl_clear(vgpu_virgl_encoder *encoder, uint32_t buffers,
                      const uint32_t rgba[4]) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CLEAR, 0U, 8U);
    vgpu_virgl_dword(encoder, buffers);
    for (index = 0U; index < 4U; ++index)
        vgpu_virgl_dword(encoder, rgba[index]);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_clear_depth(vgpu_virgl_encoder *encoder, uint32_t buffers,
                            const uint32_t rgba[4]) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CLEAR, 0U, 8U);
    vgpu_virgl_dword(encoder, buffers);
    for (index = 0U; index < 4U; ++index)
        vgpu_virgl_dword(encoder, rgba[index]);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x3ff00000U); /* double 1.0, little-endian words */
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_vertex_elements(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       uint32_t position_format,
                                       uint32_t colour_format) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_VERTEX_ELEMENTS, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, position_format);
    vgpu_virgl_dword(encoder, 16U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, colour_format);
}

void vgpu_virgl_create_vertex_elements_uv(vgpu_virgl_encoder *encoder,
                                          uint32_t handle,
                                          uint32_t position_format,
                                          uint32_t colour_format,
                                          uint32_t uv_format) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_VERTEX_ELEMENTS, 13U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, position_format);
    vgpu_virgl_dword(encoder, 16U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, colour_format);
    vgpu_virgl_dword(encoder, 32U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, uv_format);
}

void vgpu_virgl_set_vertex_buffer(vgpu_virgl_encoder *encoder,
                                  uint32_t resource, uint32_t stride) {
    vgpu_virgl_set_vertex_buffer_offset(encoder, resource, stride, 0U);
}

void vgpu_virgl_set_vertex_buffer_offset(vgpu_virgl_encoder *encoder,
                                         uint32_t resource, uint32_t stride,
                                         uint32_t offset) {
    vgpu_virgl_command(encoder, VIRGL_SET_VERTEX_BUFFERS, 0U, 3U);
    vgpu_virgl_dword(encoder, stride);
    vgpu_virgl_dword(encoder, offset);
    vgpu_virgl_dword(encoder, resource);
}

void vgpu_virgl_inline_write_buffer(vgpu_virgl_encoder *encoder,
                                    uint32_t resource, const void *data,
                                    uint32_t length) {
    uint32_t data_dwords = (length + 3U) >> 2U;
    if (!data || length == 0U || data_dwords > 0xffffU - 11U) {
        if (encoder) encoder->failed = 1;
        return;
    }
    vgpu_virgl_command(encoder, VIRGL_RESOURCE_INLINE_WRITE, 0U,
                       (uint16_t)(11U + data_dwords));
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, 0U); /* level */
    vgpu_virgl_dword(encoder, 0U); /* usage */
    /* PIPE_BUFFER transfers are one-dimensional byte ranges.  Mesa and
       virglrenderer expect stride/layer_stride to be zero for this target;
       a byte length here can make the inline write fail host-side while QEMU
       still completes the enclosing virtio command. */
    vgpu_virgl_dword(encoder, 0U); /* stride */
    vgpu_virgl_dword(encoder, 0U); /* layer stride */
    vgpu_virgl_dword(encoder, 0U); /* x */
    vgpu_virgl_dword(encoder, 0U); /* y */
    vgpu_virgl_dword(encoder, 0U); /* z */
    vgpu_virgl_dword(encoder, length); /* width in bytes */
    vgpu_virgl_dword(encoder, 1U);
    vgpu_virgl_dword(encoder, 1U);
    vgpu_virgl_bytes(encoder, data, length);
}

void vgpu_virgl_inline_write_texture(vgpu_virgl_encoder *encoder,
                                     uint32_t resource, const void *data,
                                     uint32_t width, uint32_t height,
                                     uint32_t bytes_per_pixel) {
    uint32_t length;
    uint32_t data_dwords;
    if (!data || width == 0U || height == 0U || bytes_per_pixel == 0U ||
        width > 0xffffffffU / bytes_per_pixel ||
        height > 0xffffffffU / (width * bytes_per_pixel)) {
        if (encoder) encoder->failed = 1;
        return;
    }
    length = width * height * bytes_per_pixel;
    data_dwords = (length + 3U) >> 2U;
    if (data_dwords > 0xffffU - 11U) {
        if (encoder) encoder->failed = 1;
        return;
    }
    vgpu_virgl_command(encoder, VIRGL_RESOURCE_INLINE_WRITE, 0U,
                       (uint16_t)(11U + data_dwords));
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, width * bytes_per_pixel);
    vgpu_virgl_dword(encoder, length);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, width);
    vgpu_virgl_dword(encoder, height);
    vgpu_virgl_dword(encoder, 1U);
    vgpu_virgl_bytes(encoder, data, length);
}

void vgpu_virgl_inline_write_dwords(vgpu_virgl_encoder *encoder,
                                    uint32_t resource,
                                    const uint32_t *dwords,
                                    uint32_t dword_count) {
    uint32_t index;
    uint32_t length;
    if (!dwords || dword_count == 0U || dword_count > 0xffffU - 11U) {
        if (encoder) encoder->failed = 1;
        return;
    }
    length = dword_count * 4U;
    vgpu_virgl_command(encoder, VIRGL_RESOURCE_INLINE_WRITE, 0U,
                       (uint16_t)(11U + dword_count));
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); /* PIPE_BUFFER stride */
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, length);
    vgpu_virgl_dword(encoder, 1U);
    vgpu_virgl_dword(encoder, 1U);
    for (index = 0U; index < dword_count; ++index)
        vgpu_virgl_dword(encoder, dwords[index]);
}

void vgpu_virgl_create_shader_text(vgpu_virgl_encoder *encoder,
                                   uint32_t handle, uint32_t shader_type,
                                   uint32_t token_count, const char *text) {
    uint32_t text_length = text ? (uint32_t)strlen(text) + 1U : 0U;
    uint32_t text_dwords = (text_length + 3U) >> 2U;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_SHADER,
                       (uint16_t)(5U + text_dwords));
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, shader_type);
    vgpu_virgl_dword(encoder, text_length);
    vgpu_virgl_dword(encoder, token_count);
    vgpu_virgl_dword(encoder, 0U);
    if (text_length) vgpu_virgl_bytes(encoder, text, text_length);
}

void vgpu_virgl_bind_shader(vgpu_virgl_encoder *encoder, uint32_t handle,
                            uint32_t shader_type) {
    vgpu_virgl_command(encoder, VIRGL_BIND_SHADER, 0U, 2U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, shader_type);
}

void vgpu_virgl_create_sampler_state(vgpu_virgl_encoder *encoder,
                                     uint32_t handle) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_SAMPLER_STATE, 9U);
    vgpu_virgl_dword(encoder, handle);
    /* repeat S/T/R, linear minification and magnification, no mipmaps */
    vgpu_virgl_dword(encoder, (2U << 11U) | (1U << 9U) | (1U << 13U));
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    for (index = 0U; index < 4U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_sampler_view(vgpu_virgl_encoder *encoder,
                                    uint32_t handle, uint32_t resource,
                                    uint32_t format) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_SAMPLER_VIEW, 6U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, format);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x688U); /* identity RGBA swizzle */
}

void vgpu_virgl_bind_sampler_states(vgpu_virgl_encoder *encoder,
                                    uint32_t shader_type,
                                    uint32_t first_handle,
                                    uint32_t second_handle) {
    vgpu_virgl_command(encoder, VIRGL_BIND_SAMPLER_STATES, 0U, 4U);
    vgpu_virgl_dword(encoder, shader_type);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, first_handle);
    vgpu_virgl_dword(encoder, second_handle);
}

void vgpu_virgl_set_sampler_views(vgpu_virgl_encoder *encoder,
                                  uint32_t shader_type,
                                  uint32_t first_handle,
                                  uint32_t second_handle) {
    vgpu_virgl_command(encoder, VIRGL_SET_SAMPLER_VIEWS, 0U, 4U);
    vgpu_virgl_dword(encoder, shader_type);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, first_handle);
    vgpu_virgl_dword(encoder, second_handle);
}

void vgpu_virgl_set_constant_buffer(vgpu_virgl_encoder *encoder,
                                    uint32_t shader_type,
                                    uint32_t index,
                                    const uint32_t *dwords,
                                    uint32_t dword_count) {
    uint32_t i;
    if (dword_count && !dwords) {
        if (encoder) encoder->failed = 1;
        return;
    }
    vgpu_virgl_command(encoder, VIRGL_SET_CONSTANT_BUFFER, 0U,
                       (uint16_t)(dword_count + 2U));
    vgpu_virgl_dword(encoder, shader_type);
    vgpu_virgl_dword(encoder, index);
    for (i = 0U; i < dword_count; ++i)
        vgpu_virgl_dword(encoder, dwords[i]);
}

void vgpu_virgl_create_blend(vgpu_virgl_encoder *encoder, uint32_t handle) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x78000000U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_blend_additive(vgpu_virgl_encoder *encoder,
                                      uint32_t handle) {
    unsigned int index;
    /* Gallium RT blend word: enable, ADD, SRC_ALPHA/ONE for RGB and alpha,
       with RGBA colour mask.  This matches OpenGW's dominant
       glBlendFunc(GL_SRC_ALPHA, GL_ONE) vector-light path. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x78460231U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_blend_overlay(vgpu_virgl_encoder *encoder,
                                     uint32_t handle) {
    unsigned int index;
    /* GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA.  Gallium uses DST_ALPHA for
       the alpha source factor corresponding to RGB's DST_COLOR. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x7cc82651U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_dsa(vgpu_virgl_encoder *encoder, uint32_t handle) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 6U); /* depth writemask + PIPE_FUNC_LESS */
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_dsa_depth(vgpu_virgl_encoder *encoder,
                                 uint32_t handle) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 7U); /* enable + write + PIPE_FUNC_LESS */
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_rasterizer(vgpu_virgl_encoder *encoder,
                                  uint32_t handle) {
    vgpu_virgl_create_rasterizer_sizes(encoder, handle, 0x3f800000U,
                                       0x3f800000U);
}

void vgpu_virgl_create_rasterizer_width(vgpu_virgl_encoder *encoder,
                                        uint32_t handle,
                                        uint32_t line_width_bits) {
    vgpu_virgl_create_rasterizer_sizes(encoder, handle, 0x3f800000U,
                                       line_width_bits);
}

void vgpu_virgl_create_rasterizer_sizes(vgpu_virgl_encoder *encoder,
                                        uint32_t handle,
                                        uint32_t point_size_bits,
                                        uint32_t line_width_bits) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_RASTERIZER, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0x60000002U);
    vgpu_virgl_dword(encoder, point_size_bits); /* VirGL RS S1 */
    vgpu_virgl_dword(encoder, 0U);              /* sprite coord enable */
    vgpu_virgl_dword(encoder, 0U);              /* RS S3 */
    vgpu_virgl_dword(encoder, line_width_bits); /* VirGL RS S4 */
    for (index = 0U; index < 3U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_set_viewport(vgpu_virgl_encoder *encoder,
                             const uint32_t scale[3],
                             const uint32_t translate[3]) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_SET_VIEWPORT, 0U, 7U);
    vgpu_virgl_dword(encoder, 0U);
    for (index = 0U; index < 3U; ++index) vgpu_virgl_dword(encoder, scale[index]);
    for (index = 0U; index < 3U; ++index) vgpu_virgl_dword(encoder, translate[index]);
}

void vgpu_virgl_draw_triangles(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count) {
    vgpu_virgl_draw_primitive(encoder, vertex_count, 4U);
}

void vgpu_virgl_draw_primitive(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count,
                               uint32_t primitive_mode) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_DRAW_VBO, 0U, 12U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, vertex_count);
    vgpu_virgl_dword(encoder, primitive_mode);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 1U);
    for (index = 0U; index < 5U; ++index) vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0xffffffffU);
    vgpu_virgl_dword(encoder, 0U);
}
