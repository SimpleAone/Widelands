#include "graphic/virtio/virgl_encoder.h"

#include <stddef.h>
#include <string.h>

/* The scissor bit in every rasterizer, switchable because it is the one
   change that affects every draw at once: if the rectangle or its
   orientation is wrong, nothing is drawn anywhere. The backend sets this
   from VIRTIOGL_SCISSOR before it builds the session. */
static uint32_t scissorBit;

void vgpu_virgl_scissor_bit(int on) { scissorBit = on ? (1U << 14) : 0U; }

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
    VIRGL_SET_INDEX_BUFFER = 11,
    VIRGL_SET_CONSTANT_BUFFER = 12,
    VIRGL_RESOURCE_COPY_REGION = 17,
    VIRGL_BIND_SAMPLER_STATES = 18,
    VIRGL_SET_STENCIL_REF = 13,
    VIRGL_SET_SCISSOR_STATE = 15,
    VIRGL_SET_CLIP_STATE = 23,
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

void vgpu_virgl_clear_depth_value(vgpu_virgl_encoder *encoder,
                                  uint32_t buffers, const uint32_t rgba[4],
                                  uint32_t depth_low, uint32_t depth_high) {
    unsigned int index;
    /* VIRGL_CCMD_CLEAR payload: buffers mask, four colour dwords, the depth
       value as a double (low half first), then stencil. */
    vgpu_virgl_command(encoder, VIRGL_CLEAR, 0U, 8U);
    vgpu_virgl_dword(encoder, buffers);
    for (index = 0U; index < 4U; ++index)
        vgpu_virgl_dword(encoder, rgba[index]);
    vgpu_virgl_dword(encoder, depth_low);
    vgpu_virgl_dword(encoder, depth_high);
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_clear(vgpu_virgl_encoder *encoder, uint32_t buffers,
                      const uint32_t rgba[4]) {
    vgpu_virgl_clear_depth_value(encoder, buffers, rgba, 0U, 0U);
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

void vgpu_virgl_create_vertex_elements_uv2(vgpu_virgl_encoder *encoder,
                                           uint32_t handle,
                                           uint32_t position_format,
                                           uint32_t colour_format,
                                           uint32_t uv_format) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_VERTEX_ELEMENTS, 17U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, position_format);
    vgpu_virgl_dword(encoder, 16U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, colour_format);
    vgpu_virgl_dword(encoder, 32U); vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U); vgpu_virgl_dword(encoder, uv_format);
    vgpu_virgl_dword(encoder, 40U); vgpu_virgl_dword(encoder, 0U);
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

/* Host-side resource-to-resource copy: what glCopyTexSubImage2D needs, and
   the reason GLQuake's animated water was switched off here. The engine
   draws the warp geometry, lifts the result into the texture with this, and
   R_Clear() wipes the framebuffer immediately afterwards -- so nothing of it
   reaches the screen directly. Nothing travels back to the guest either;
   both resources live on the host.

   Payload is 13 dwords, the layout virgl_protocol.h calls
   VIRGL_CMD_RESOURCE_COPY_REGION. */
void vgpu_virgl_copy_region(vgpu_virgl_encoder *encoder,
                            uint32_t destination, uint32_t destination_level,
                            uint32_t destination_x, uint32_t destination_y,
                            uint32_t destination_z, uint32_t source,
                            uint32_t source_level, uint32_t source_x,
                            uint32_t source_y, uint32_t source_z,
                            uint32_t width, uint32_t height, uint32_t depth) {
    vgpu_virgl_command(encoder, VIRGL_RESOURCE_COPY_REGION, 0U, 13U);
    vgpu_virgl_dword(encoder, destination);
    vgpu_virgl_dword(encoder, destination_level);
    vgpu_virgl_dword(encoder, destination_x);
    vgpu_virgl_dword(encoder, destination_y);
    vgpu_virgl_dword(encoder, destination_z);
    vgpu_virgl_dword(encoder, source);
    vgpu_virgl_dword(encoder, source_level);
    vgpu_virgl_dword(encoder, source_x);
    vgpu_virgl_dword(encoder, source_y);
    vgpu_virgl_dword(encoder, source_z);
    vgpu_virgl_dword(encoder, width);
    vgpu_virgl_dword(encoder, height);
    vgpu_virgl_dword(encoder, depth);
}

void vgpu_virgl_create_sampler_state_wrap(vgpu_virgl_encoder *encoder,
                                          uint32_t handle, uint32_t wrap,
                                          uint32_t mip_linear) {
    unsigned int index;
    /* S0: bits 0-2 wrap_s, 3-5 wrap_t, 6-8 wrap_r, 9-10 min filter,
       11-12 mip filter, 13-14 mag filter (vrend_decode_create_sampler_state).
       `wrap` is a PIPE_TEX_WRAP_*: 0 REPEAT, 2 CLAMP_TO_EDGE. Filters are
       linear, mip filter included (PIPE_TEX_MIPFILTER_LINEAR = 0): Quake
       builds a full mip chain for its world textures and this is what
       spends it. With mipmapping off, a 64x64 wall seen at a distance was
       minified into a handful of pixels straight off the base level, and
       the scattered texels that picks read as a mosaic of noise -- always
       the same surfaces, always distance-dependent.

       max_lod has to admit the levels as well, and so does each sampler
       view's last_level.

       GLQuake picks the wrap mode per texture kind (gl_textures.c): world
       textures repeat, but 2D pictures and model skins clamp. With one
       repeating sampler for everything, their edge pixels sample from the
       opposite side -- the thin wrong-coloured lines along HUD, menu and
       skin edges. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_SAMPLER_STATE, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, ((mip_linear ? 0U : 2U) << 11U) |
                              (1U << 9U) | (1U << 13U) |
                              (wrap & 7U) | ((wrap & 7U) << 3) |
                              ((wrap & 7U) << 6));
    vgpu_virgl_dword(encoder, 0U);                    /* lod_bias */
    vgpu_virgl_dword(encoder, 0U);                    /* min_lod 0.0f */
    /* max_lod has to admit the levels; min_lod stays at 0.0 so the pair
       can never be inverted. */
    vgpu_virgl_dword(encoder, mip_linear ? 0x41700000U : 0U); /* 15.0f */
    for (index = 0U; index < 4U; ++index) vgpu_virgl_dword(encoder, 0U);
}

/* Full control over the three filters, for the sampler table the draw loop
   selects from. `min_img`/`mag_img` are PIPE_TEX_FILTER_* (0 nearest, 1
   linear) and `mip` is PIPE_TEX_MIPFILTER_* (0 nearest, 1 linear, 2 none),
   which is exactly how GL_TEXTURE_MIN_FILTER decomposes. */
void vgpu_virgl_create_sampler_state_filter(vgpu_virgl_encoder *encoder,
                                            uint32_t handle, uint32_t wrap,
                                            uint32_t min_img, uint32_t mip,
                                            uint32_t mag_img) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_SAMPLER_STATE, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, ((mip & 3U) << 11U) |
                              ((min_img & 3U) << 9U) |
                              ((mag_img & 3U) << 13U) |
                              (wrap & 7U) | ((wrap & 7U) << 3) |
                              ((wrap & 7U) << 6));
    vgpu_virgl_dword(encoder, 0U);                    /* lod_bias */
    vgpu_virgl_dword(encoder, 0U);                    /* min_lod 0.0f */
    vgpu_virgl_dword(encoder, mip == 2U ? 0U : 0x41700000U); /* max_lod */
    for (index = 0U; index < 4U; ++index) vgpu_virgl_dword(encoder, 0U);
}
void vgpu_virgl_create_sampler_state(vgpu_virgl_encoder *encoder,
                                     uint32_t handle) {
    vgpu_virgl_create_sampler_state_wrap(encoder, handle, 0U, 0U);
}

void vgpu_virgl_create_sampler_view(vgpu_virgl_encoder *encoder,
                                    uint32_t handle, uint32_t resource,
                                    uint32_t format, uint32_t last_level) {
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_SAMPLER_VIEW, 6U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, format);
    /* Two separate fields, and they are easy to swap: dword 4 is
       first_layer | last_layer << 16, dword 5 is first_level |
       last_level << 8. Putting the levels in the first one sets a layer
       range on a plain 2D texture, which the host rejects with
       GL_INVALID_VALUE and a failed CREATE_OBJECT. */
    vgpu_virgl_dword(encoder, 0U);                      /* layers 0..0 */
    vgpu_virgl_dword(encoder, (last_level & 0xffU) << 8);
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

/* Any source/destination factor pair, as Gallium PIPE_BLENDFACTOR values.
   The RT word is enable | func<<1 | src<<4 | dst<<9 | alphafunc<<14 |
   alphasrc<<17 | alphadst<<22 | colormask<<27, with ADD (0) for both
   equations and the alpha channel blended the same way as the colour --
   which is what fixed-function GL does.

   This exists so the compatibility layer never has to *recognise* a blend
   mode again. Matching a handful of known pairs and silently drawing
   everything else opaque is what hid Hexen II's lightmaps once and Quake
   III's twice: the second time the unrecognised pair was
   GL_DST_COLOR/GL_ZERO, mathematically identical to a pair already in the
   table but spelled the other way round. */
void vgpu_virgl_create_blend_factors(vgpu_virgl_encoder *encoder,
                                     uint32_t handle,
                                     uint32_t src_factor,
                                     uint32_t dst_factor) {
    unsigned int index;
    uint32_t rt = 1U |
                  ((src_factor & 0x1fU) << 4) |
                  ((dst_factor & 0x1fU) << 9) |
                  ((src_factor & 0x1fU) << 17) |
                  ((dst_factor & 0x1fU) << 22) |
                  (0xfU << 27);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, rt);
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

/* Gallium RT blend word layout, from virgl_protocol.h's
   VIRGL_OBJ_BLEND_S2_* macros: bit 0 enable, bits 1-3 rgb func, 4-8 rgb
   src factor, 9-13 rgb dst factor, 14-16 alpha func, 17-21 alpha src,
   22-26 alpha dst, 27-30 colour mask. Factors are PIPE_BLENDFACTOR_*
   (ONE=0x1, SRC_COLOR=0x2, SRC_ALPHA=0x3, ZERO=0x11, INV_SRC_ALPHA=0x13);
   func 0 is PIPE_BLEND_ADD. Verified by decoding the two words that were
   already here and known good. */
void vgpu_virgl_create_blend_modulate(vgpu_virgl_encoder *encoder,
                                      uint32_t handle) {
    unsigned int index;
    /* glBlendFunc(GL_ZERO, GL_SRC_COLOR) -- multiplies what is already in
       the framebuffer by the incoming colour. Quake's lightmap pass, which
       without this fell back to opaque and simply overwrote the textured
       world with the (mostly dark) lightmap. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x78A20511U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_blend_inv_modulate(vgpu_virgl_encoder *encoder,
                                          uint32_t handle) {
    unsigned int index;
    /* glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR) -- Hexen II's lightmap
       pass when gl_lightmap_format is GL_LUMINANCE (R_BlendLightmaps in
       gl_rsurf.c). Quake uses GL_ZERO/GL_SRC_COLOR instead, which is why
       tyrquake needed create_blend_modulate and never hit this one; an
       unrecognised pair falls back to opaque in currentBlendMode(), and an
       opaque lightmap pass overwrites the textured world with the lightmap
       rather than modulating it -- the whole world black while models and
       the HUD, which are single-pass, still show.

       Same word as create_blend_modulate with both destination factors
       changed from PIPE_BLENDFACTOR_SRC_COLOR (0x2) to INV_SRC_COLOR
       (0x12): bit 13 for the rgb field at bits 9-13, bit 26 for the alpha
       field at bits 22-26. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x7CA22511U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_blend_alpha(vgpu_virgl_encoder *encoder,
                                   uint32_t handle) {
    unsigned int index;
    /* glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) -- ordinary
       transparency (water surfaces, the console overlay). */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x7CC62631U);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_blend_add_one(vgpu_virgl_encoder *encoder,
                                     uint32_t handle) {
    unsigned int index;
    /* glBlendFunc(GL_ONE, GL_ONE) -- unweighted additive, used for
       fullbright/glow passes. Distinct from create_blend_additive, which is
       SRC_ALPHA/ONE. */
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x78420211U);
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

void vgpu_virgl_create_dsa_func(vgpu_virgl_encoder *encoder, uint32_t handle,
                                uint32_t func, int write_enable) {
    /* DSA S0: bit 0 depth enable, bit 1 depth writemask, bits 2-4 func.
       The two fixed states below are this with (func=LESS, write=1) and
       enable clear or set. */
    uint32_t state = 1U | (write_enable ? 2U : 0U) | ((func & 7U) << 2);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, state);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

/* Same object with the alpha test switched on. Gallium folds alpha test
   into the depth/stencil/alpha state: S0 bit 8 enables it, bits 9-11 hold
   the PIPE_FUNC, and the reference value is the float in the last dword
   (vrend_decode_create_dsa reads it with uif()). GLQuake needs this for
   sprites -- explosions, flames, the lightning bolt -- which are quads
   with a fully transparent border; without the test they render as opaque
   rectangles. */
void vgpu_virgl_create_dsa_alpha(vgpu_virgl_encoder *encoder, uint32_t handle,
                                 int depth_enable, uint32_t func,
                                 int write_enable, uint32_t alpha_func,
                                 float alpha_ref) {
    union { float f; uint32_t u; } reference;
    uint32_t state = (depth_enable ? 1U : 0U) | (write_enable ? 2U : 0U) |
                     ((func & 7U) << 2) | (1U << 8) |
                     ((alpha_func & 7U) << 9);
    reference.f = alpha_ref;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, state);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, reference.u);
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

/* Blend state with an explicit colour write mask and blending off.
   Gallium's RT0 word puts the mask in bits 27-30 (R,G,B,A). A mask of 0
   writes no colour at all, which is what glColorMask(0,0,0,0) asks for --
   Quake III uses it for depth-only and shadow-volume passes, where the
   blend factors are irrelevant precisely because nothing is written. */
void vgpu_virgl_create_blend_mask(vgpu_virgl_encoder *encoder,
                                  uint32_t handle, uint32_t colour_mask) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, (colour_mask & 0xfU) << 27);
    for (index = 1U; index < 8U; ++index) vgpu_virgl_dword(encoder, 0U);
}

/* The full rasterizer: face culling, winding, and polygon offset alongside
   the point and line sizes the sized variant already carried.

   S0 bits, from virgl_protocol.h: 8-9 cull face (PIPE_FACE_NONE 0, FRONT 1,
   BACK 2), 15 front-face-is-counter-clockwise, 20 offset for filled
   triangles. Bit 1 is depth clip and bits 29-30 are half-pixel-centre and
   bottom-edge rule, which every rasterizer here has always set.

   The last three dwords are offset units, scale and clamp. GL names them
   the other way round from Gallium: glPolygonOffset(factor, units) maps to
   scale=factor and units=units, and GL's units are in depth-buffer
   quanta while Gallium wants the same number, so they pass through. */
/* Stencil, as a depth/stencil/alpha object plus a separate reference
   value. The reference is not part of the object: VirGL carries it in its
   own command, so two draws differing only in glStencilFunc's ref share
   one object.

   Bit layout is virgl_protocol.h's, not guessed: S1 holds enable (0),
   func (1-3), fail op (4-6), zpass op (7-9), zfail op (10-12), value mask
   (13-20) and write mask (21-28). S2 is the back face, and gets the same
   state -- glStencilOp and glStencilFunc without the Separate suffix set
   both faces, which is all any client here uses. */
void vgpu_virgl_create_dsa_stencil(vgpu_virgl_encoder *encoder,
                                   uint32_t handle,
                                   int depth_enable, uint32_t depth_func,
                                   int depth_write,
                                   uint32_t stencil_func,
                                   uint32_t fail_op, uint32_t zfail_op,
                                   uint32_t zpass_op,
                                   uint32_t value_mask,
                                   uint32_t write_mask) {
    uint32_t s0 = (depth_enable ? 1U : 0U) |
                  ((depth_write ? 1U : 0U) << 1) |
                  ((depth_func & 7U) << 2);
    uint32_t s1 = 1U |
                  ((stencil_func & 7U) << 1) |
                  ((fail_op & 7U) << 4) |
                  ((zpass_op & 7U) << 7) |
                  ((zfail_op & 7U) << 10) |
                  ((value_mask & 0xffU) << 13) |
                  ((write_mask & 0xffU) << 21);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, s0);
    vgpu_virgl_dword(encoder, s1);
    vgpu_virgl_dword(encoder, s1);            /* back face, same state */
    vgpu_virgl_dword(encoder, 0U);            /* alpha ref, unused here */
}

void vgpu_virgl_set_stencil_ref(vgpu_virgl_encoder *encoder,
                                uint32_t front, uint32_t back) {
    vgpu_virgl_command(encoder, VIRGL_SET_STENCIL_REF, 0U, 1U);
    vgpu_virgl_dword(encoder, (front & 0xffU) | ((back & 0xffU) << 8));
}

/* A rasterizer that draws lines instead of filled triangles, or shades
   flat instead of smooth. PIPE_POLYGON_MODE_LINE is 1, and it has to be
   set for both faces; flatshade is S0 bit 0. Neither combines with the
   culling, offset or clipping variants -- they are diagnostics
   (r_showtris) and a mode no client here actually selects, so one object
   each is enough. */
void vgpu_virgl_create_rasterizer_mode(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       int wireframe, int flatshade) {
    uint32_t state = 0x60000002U | 0x06800000U | scissorBit |
                     (flatshade ? 1U : 0U) |
                     (wireframe ? ((1U << 10) | (1U << 12)) : 0U);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_RASTERIZER, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, state);
    vgpu_virgl_dword(encoder, 0x3f800000U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0x3f800000U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

/* One scissor rectangle, in slot 0. The payload is a start slot followed
   by two packed dwords per rectangle: minx | miny<<16 and maxx | maxy<<16.

   Note that the rectangle alone does nothing -- the rasterizer's S0
   scissor bit is what turns the test on, and this backend sets that bit on
   every rasterizer it creates. That is deliberate: Quake III enables
   GL_SCISSOR_TEST for essentially every 3D pass, so making the enable a
   per-object property would double the rasterizer table for no gain. With
   the bit always on, "no scissor" is simply the whole frame target. */
void vgpu_virgl_set_scissor(vgpu_virgl_encoder *encoder,
                            uint32_t minx, uint32_t miny,
                            uint32_t maxx, uint32_t maxy) {
    vgpu_virgl_command(encoder, VIRGL_SET_SCISSOR_STATE, 0U, 3U);
    vgpu_virgl_dword(encoder, 0U);                       /* start slot */
    vgpu_virgl_dword(encoder, (minx & 0xffffU) | ((miny & 0xffffU) << 16));
    vgpu_virgl_dword(encoder, (maxx & 0xffffU) | ((maxy & 0xffffU) << 16));
}

/* User clip planes. VIRGL_CCMD_SET_CLIP_STATE carries all eight planes as
   32 dwords whether or not they are used, and which of them actually clip
   is decided by the rasterizer's S3 clip_plane_enable bits -- see
   vgpu_virgl_create_rasterizer_clipped(). Only plane 0 is ever filled
   here: Quake III uses GL_CLIP_PLANE0 and nothing else does.

   The planes are in clip space, not eye space as glClipPlane's are. The
   caller does that conversion; see the comment on glClipPlane in
   virtgl_compat.cpp for the matrices involved. */
void vgpu_virgl_set_clip_state(vgpu_virgl_encoder *encoder,
                               const uint32_t plane0_bits[4]) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_SET_CLIP_STATE, 0U, 32U);
    for (index = 0U; index < 4U; ++index)
        vgpu_virgl_dword(encoder, plane0_bits[index]);
    for (index = 4U; index < 32U; ++index)
        vgpu_virgl_dword(encoder, 0U);
}

/* As create_rasterizer_full, plus the clip_plane_enable mask that decides
   which of the eight planes set above are applied. It sits in the S3
   dword, which the other rasterizer builders leave at zero. */
void vgpu_virgl_create_rasterizer_clipped(vgpu_virgl_encoder *encoder,
                                          uint32_t handle,
                                          uint32_t point_size_bits,
                                          uint32_t line_width_bits,
                                          uint32_t cull_face,
                                          int front_ccw,
                                          uint32_t clip_plane_enable) {
    uint32_t state = 0x60000002U | 0x06800000U | scissorBit |
                     ((cull_face & 3U) << 8) |
                     (front_ccw ? (1U << 15) : 0U);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_RASTERIZER, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, state);
    vgpu_virgl_dword(encoder, point_size_bits);
    vgpu_virgl_dword(encoder, 0U);                        /* sprite coord */
    vgpu_virgl_dword(encoder, (clip_plane_enable & 0xffU) << 24); /* S3 */
    vgpu_virgl_dword(encoder, line_width_bits);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0U);
}

void vgpu_virgl_create_rasterizer_full(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       uint32_t point_size_bits,
                                       uint32_t line_width_bits,
                                       uint32_t cull_face,
                                       int front_ccw,
                                       int offset_tri,
                                       uint32_t offset_units_bits,
                                       uint32_t offset_scale_bits) {
    uint32_t state = 0x60000002U | scissorBit |
                     ((cull_face & 3U) << 8) |
                     (front_ccw ? (1U << 15) : 0U) |
                     (offset_tri ? (1U << 20) : 0U);
    vgpu_virgl_command(encoder, VIRGL_CREATE_OBJECT,
                       VIRGL_OBJECT_RASTERIZER, 9U);
    vgpu_virgl_dword(encoder, handle);
    vgpu_virgl_dword(encoder, state);
    vgpu_virgl_dword(encoder, point_size_bits);
    vgpu_virgl_dword(encoder, 0U);              /* sprite coord enable */
    vgpu_virgl_dword(encoder, 0U);              /* RS S3: line stipple */
    vgpu_virgl_dword(encoder, line_width_bits);
    vgpu_virgl_dword(encoder, offset_units_bits);
    vgpu_virgl_dword(encoder, offset_scale_bits);
    vgpu_virgl_dword(encoder, 0U);              /* offset clamp */
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
    /*
     * VirGL rasterizer state S0 flags dword. Base value (0x60000002) is
     * depth_clip + half_pixel_center + bottom_edge_rule (D3D-style
     * coordinate convention). OR'd in here: point_smooth (bit 23),
     * multisample (bit 25), line_smooth (bit 26) -- confirmed against
     * virglrenderer's src/vrend/vrend_decode.c decode order. Without these,
     * every line and point this driver draws -- grid, particles, stars, and
     * the vector font text/score/UI, all built from GL_LINES -- rasterizes
     * hard-edged instead of anti-aliased, visibly thicker/coarser than the
     * native Windows/Linux OpenGL build. Applied globally rather than
     * per-draw: the game's own GL_LINE_SMOOTH/GL_POINT_SMOOTH toggles are
     * presently no-ops in virtgl_compat.cpp (a separate, larger fix -- see
     * project_virtio_gpu_missing_smoothing in the assistant's memory), so
     * unconditional is the only option that actually reaches the GPU today.
     */
    vgpu_virgl_dword(encoder, 0x60000002U | 0x06800000U | scissorBit);
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

/* Draws from a first vertex rather than from zero.
 *
 * The start field was always written as 0, so a caller drawing successive
 * runs out of one vertex buffer had to rebind that buffer at a new offset
 * before every draw -- an extra command, and on the host an extra vertex
 * buffer state change, for something the draw itself can express. */
void vgpu_virgl_draw_primitive_from(vgpu_virgl_encoder *encoder,
                                    uint32_t start,
                                    uint32_t vertex_count,
                                    uint32_t primitive_mode) {
    unsigned int index;
    vgpu_virgl_command(encoder, VIRGL_DRAW_VBO, 0U, 12U);
    vgpu_virgl_dword(encoder, start);
    vgpu_virgl_dword(encoder, vertex_count);
    vgpu_virgl_dword(encoder, primitive_mode);
    vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 1U);
    for (index = 0U; index < 5U; ++index) vgpu_virgl_dword(encoder, 0U);
    vgpu_virgl_dword(encoder, 0xffffffffU);
    vgpu_virgl_dword(encoder, 0U);
}

/* Binds the resource backing subsequent indexed draws. index_size is bytes
   per index (1, 2 or 4 -- glDrawElements' GL_UNSIGNED_BYTE/SHORT/INT). */
void vgpu_virgl_set_index_buffer(vgpu_virgl_encoder *encoder,
                                 uint32_t resource, uint32_t index_size,
                                 uint32_t offset) {
    vgpu_virgl_command(encoder, VIRGL_SET_INDEX_BUFFER, 0U, 3U);
    vgpu_virgl_dword(encoder, resource);
    vgpu_virgl_dword(encoder, index_size);
    vgpu_virgl_dword(encoder, offset);
}

/* Same VIRGL_DRAW_VBO layout as vgpu_virgl_draw_primitive() with the
   "indexed" field set: count is now an index count (read through whatever
   vgpu_virgl_set_index_buffer() last bound), and base_vertex offsets every
   resolved index into the currently bound vertex buffer -- lets an indexed
   batch share the same per-frame vertex buffer as everything else, drawn
   from its own offset, the same way first_vertex does for non-indexed
   batches. min/max index bounds are left permissive (0..UINT32_MAX):
   virglrenderer treats them as an optional validation/optimisation hint,
   not a hard requirement. */
void vgpu_virgl_draw_primitive(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count,
                               uint32_t primitive_mode) {
    vgpu_virgl_draw_primitive_from(encoder, 0U, vertex_count, primitive_mode);
}

void vgpu_virgl_draw_primitive_indexed(vgpu_virgl_encoder *encoder,
                                       uint32_t index_count,
                                       uint32_t primitive_mode,
                                       uint32_t base_vertex) {
    vgpu_virgl_command(encoder, VIRGL_DRAW_VBO, 0U, 12U);
    vgpu_virgl_dword(encoder, 0U);            /* start */
    vgpu_virgl_dword(encoder, index_count);   /* count */
    vgpu_virgl_dword(encoder, primitive_mode);
    vgpu_virgl_dword(encoder, 1U);            /* indexed */
    vgpu_virgl_dword(encoder, 1U);            /* instance_count */
    vgpu_virgl_dword(encoder, base_vertex);   /* index_bias */
    vgpu_virgl_dword(encoder, 0U);            /* start_instance */
    vgpu_virgl_dword(encoder, 0U);            /* primitive_restart */
    vgpu_virgl_dword(encoder, 0U);            /* restart_index */
    vgpu_virgl_dword(encoder, 0U);            /* min_index */
    vgpu_virgl_dword(encoder, 0xffffffffU);   /* max_index */
    vgpu_virgl_dword(encoder, 0U);            /* pad */
}
