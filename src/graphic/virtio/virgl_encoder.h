#ifndef VGPU_VIRGL_ENCODER_H
#define VGPU_VIRGL_ENCODER_H

#include <stdint.h>

typedef struct vgpu_virgl_encoder {
    uint8_t *bytes;
    uint32_t capacity;
    uint32_t length;
    int failed;
} vgpu_virgl_encoder;

void vgpu_virgl_encoder_init(vgpu_virgl_encoder *encoder, void *buffer,
                             uint32_t capacity);
int vgpu_virgl_encoder_ok(const vgpu_virgl_encoder *encoder);
uint32_t vgpu_virgl_encoder_size(const vgpu_virgl_encoder *encoder);
void vgpu_virgl_dword(vgpu_virgl_encoder *encoder, uint32_t value);
void vgpu_virgl_bytes(vgpu_virgl_encoder *encoder, const void *bytes,
                      uint32_t length);
void vgpu_virgl_command(vgpu_virgl_encoder *encoder, uint8_t command,
                        uint8_t object, uint16_t payload_dwords);
void vgpu_virgl_bind_object(vgpu_virgl_encoder *encoder, uint8_t object,
                            uint32_t handle);
void vgpu_virgl_destroy_object(vgpu_virgl_encoder *encoder, uint8_t object,
                               uint32_t handle);
void vgpu_virgl_create_surface(vgpu_virgl_encoder *encoder, uint32_t handle,
                               uint32_t resource, uint32_t format);
void vgpu_virgl_create_surface_layer(vgpu_virgl_encoder *encoder,
                                     uint32_t handle, uint32_t resource,
                                     uint32_t format, uint32_t layer);
void vgpu_virgl_set_framebuffer(vgpu_virgl_encoder *encoder,
                                uint32_t colour_surface);
void vgpu_virgl_set_framebuffer_depth(vgpu_virgl_encoder *encoder,
                                      uint32_t colour_surface,
                                      uint32_t depth_surface);
/* Same as vgpu_virgl_clear but sets the depth clear value, which the
   plain version leaves at 0.0. With PIPE_FUNC_LESS depth testing a 0.0
   clear rejects every fragment, so a depth-enabled framebuffer has to
   clear to 1.0. The value is a double in the wire format, hence the two
   halves. */
void vgpu_virgl_clear_depth_value(vgpu_virgl_encoder *encoder,
                                  uint32_t buffers, const uint32_t rgba[4],
                                  uint32_t depth_low, uint32_t depth_high);
void vgpu_virgl_clear(vgpu_virgl_encoder *encoder, uint32_t buffers,
                      const uint32_t rgba[4]);
void vgpu_virgl_clear_depth(vgpu_virgl_encoder *encoder, uint32_t buffers,
                            const uint32_t rgba[4]);
void vgpu_virgl_create_vertex_elements(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       uint32_t position_format,
                                       uint32_t colour_format);
void vgpu_virgl_create_vertex_elements_uv(vgpu_virgl_encoder *encoder,
                                          uint32_t handle,
                                          uint32_t position_format,
                                          uint32_t colour_format,
                                          uint32_t uv_format);
void vgpu_virgl_create_vertex_elements_uv2(vgpu_virgl_encoder *encoder,
                                           uint32_t handle,
                                           uint32_t position_format,
                                           uint32_t colour_format,
                                           uint32_t uv_format);
void vgpu_virgl_set_vertex_buffer(vgpu_virgl_encoder *encoder,
                                  uint32_t resource, uint32_t stride);
void vgpu_virgl_set_vertex_buffer_offset(vgpu_virgl_encoder *encoder,
                                         uint32_t resource, uint32_t stride,
                                         uint32_t offset);
void vgpu_virgl_inline_write_buffer(vgpu_virgl_encoder *encoder,
                                    uint32_t resource, const void *data,
                                    uint32_t length);
void vgpu_virgl_inline_write_texture(vgpu_virgl_encoder *encoder,
                                     uint32_t resource, const void *data,
                                     uint32_t width, uint32_t height,
                                     uint32_t bytes_per_pixel);
void vgpu_virgl_inline_write_dwords(vgpu_virgl_encoder *encoder,
                                    uint32_t resource,
                                    const uint32_t *dwords,
                                    uint32_t dword_count);
void vgpu_virgl_create_shader_text(vgpu_virgl_encoder *encoder,
                                   uint32_t handle, uint32_t shader_type,
                                   uint32_t token_count, const char *text);
void vgpu_virgl_bind_shader(vgpu_virgl_encoder *encoder, uint32_t handle,
                            uint32_t shader_type);
void vgpu_virgl_copy_region(vgpu_virgl_encoder *encoder,
                            uint32_t destination, uint32_t destination_level,
                            uint32_t destination_x, uint32_t destination_y,
                            uint32_t destination_z, uint32_t source,
                            uint32_t source_level, uint32_t source_x,
                            uint32_t source_y, uint32_t source_z,
                            uint32_t width, uint32_t height, uint32_t depth);
void vgpu_virgl_create_sampler_state_wrap(vgpu_virgl_encoder *encoder,
                                          uint32_t handle, uint32_t wrap,
                                          uint32_t mip_linear);
void vgpu_virgl_create_sampler_state_filter(vgpu_virgl_encoder *encoder,
                                            uint32_t handle, uint32_t wrap,
                                            uint32_t min_img, uint32_t mip,
                                            uint32_t mag_img);
void vgpu_virgl_create_sampler_state(vgpu_virgl_encoder *encoder,
                                     uint32_t handle);
void vgpu_virgl_create_sampler_view(vgpu_virgl_encoder *encoder,
                                    uint32_t handle, uint32_t resource,
                                    uint32_t format, uint32_t last_level);
void vgpu_virgl_bind_sampler_states(vgpu_virgl_encoder *encoder,
                                    uint32_t shader_type,
                                    uint32_t first_handle,
                                    uint32_t second_handle);
void vgpu_virgl_set_sampler_views(vgpu_virgl_encoder *encoder,
                                  uint32_t shader_type,
                                  uint32_t first_handle,
                                  uint32_t second_handle);
void vgpu_virgl_set_constant_buffer(vgpu_virgl_encoder *encoder,
                                    uint32_t shader_type,
                                    uint32_t index,
                                    const uint32_t *dwords,
                                    uint32_t dword_count);
void vgpu_virgl_create_blend(vgpu_virgl_encoder *encoder, uint32_t handle);
void vgpu_virgl_create_blend_factors(vgpu_virgl_encoder *encoder,
                                     uint32_t handle,
                                     uint32_t src_factor,
                                     uint32_t dst_factor);
void vgpu_virgl_create_blend_additive(vgpu_virgl_encoder *encoder,
                                      uint32_t handle);
/* Blend states a real GL client needs beyond OpenGW's two. Named after the
   glBlendFunc pair each one encodes, since that is how virtgl_compat.cpp
   selects them. */
/* glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR): Hexen II's lightmap pass
   with GL_LUMINANCE lightmaps. */
void vgpu_virgl_create_blend_inv_modulate(vgpu_virgl_encoder *encoder,
                                          uint32_t handle);
void vgpu_virgl_create_blend_modulate(vgpu_virgl_encoder *encoder,
                                      uint32_t handle);
void vgpu_virgl_create_blend_alpha(vgpu_virgl_encoder *encoder,
                                   uint32_t handle);
void vgpu_virgl_create_blend_add_one(vgpu_virgl_encoder *encoder,
                                     uint32_t handle);
void vgpu_virgl_create_blend_overlay(vgpu_virgl_encoder *encoder,
                                     uint32_t handle);
void vgpu_virgl_create_dsa(vgpu_virgl_encoder *encoder, uint32_t handle);
/* Depth/stencil state for an arbitrary comparison and write mask.
   `func` is a PIPE_FUNC_* value (NEVER=0, LESS=1, EQUAL=2, LEQUAL=3,
   GREATER=4, NOTEQUAL=5, GEQUAL=6, ALWAYS=7), which is exactly
   glDepthFunc's argument minus GL_NEVER. */
void vgpu_virgl_create_dsa_alpha(vgpu_virgl_encoder *encoder,
                                 uint32_t handle, int depth_enable,
                                 uint32_t func, int write_enable,
                                 uint32_t alpha_func, float alpha_ref);
void vgpu_virgl_create_blend_mask(vgpu_virgl_encoder *encoder,
                                  uint32_t handle, uint32_t colour_mask);
void vgpu_virgl_create_dsa_stencil(vgpu_virgl_encoder *encoder,
                                   uint32_t handle,
                                   int depth_enable, uint32_t depth_func,
                                   int depth_write,
                                   uint32_t stencil_func,
                                   uint32_t fail_op, uint32_t zfail_op,
                                   uint32_t zpass_op,
                                   uint32_t value_mask,
                                   uint32_t write_mask);
void vgpu_virgl_set_stencil_ref(vgpu_virgl_encoder *encoder,
                                uint32_t front, uint32_t back);
void vgpu_virgl_create_rasterizer_mode(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       int wireframe, int flatshade);
void vgpu_virgl_scissor_bit(int on);
void vgpu_virgl_set_scissor(vgpu_virgl_encoder *encoder,
                            uint32_t minx, uint32_t miny,
                            uint32_t maxx, uint32_t maxy);
void vgpu_virgl_set_clip_state(vgpu_virgl_encoder *encoder,
                               const uint32_t plane0_bits[4]);
void vgpu_virgl_create_rasterizer_clipped(vgpu_virgl_encoder *encoder,
                                          uint32_t handle,
                                          uint32_t point_size_bits,
                                          uint32_t line_width_bits,
                                          uint32_t cull_face,
                                          int front_ccw,
                                          uint32_t clip_plane_enable);
void vgpu_virgl_create_rasterizer_full(vgpu_virgl_encoder *encoder,
                                       uint32_t handle,
                                       uint32_t point_size_bits,
                                       uint32_t line_width_bits,
                                       uint32_t cull_face,
                                       int front_ccw,
                                       int offset_tri,
                                       uint32_t offset_units_bits,
                                       uint32_t offset_scale_bits);
void vgpu_virgl_create_dsa_func(vgpu_virgl_encoder *encoder, uint32_t handle,
                                uint32_t func, int write_enable);
void vgpu_virgl_create_dsa_depth(vgpu_virgl_encoder *encoder,
                                 uint32_t handle);
void vgpu_virgl_create_rasterizer(vgpu_virgl_encoder *encoder,
                                  uint32_t handle);
void vgpu_virgl_create_rasterizer_width(vgpu_virgl_encoder *encoder,
                                        uint32_t handle,
                                        uint32_t line_width_bits);
void vgpu_virgl_create_rasterizer_sizes(vgpu_virgl_encoder *encoder,
                                        uint32_t handle,
                                        uint32_t point_size_bits,
                                        uint32_t line_width_bits);
void vgpu_virgl_set_viewport(vgpu_virgl_encoder *encoder,
                             const uint32_t scale[3],
                             const uint32_t translate[3]);
void vgpu_virgl_draw_triangles(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count);
/* Draws from `start` rather than from vertex zero, so a caller with many
   runs in one vertex buffer need not rebind it between draws. */
void vgpu_virgl_draw_primitive_from(vgpu_virgl_encoder *encoder,
                                    uint32_t start,
                                    uint32_t vertex_count,
                                    uint32_t primitive_mode);

void vgpu_virgl_draw_primitive(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count,
                               uint32_t primitive_mode);
void vgpu_virgl_set_index_buffer(vgpu_virgl_encoder *encoder,
                                 uint32_t resource, uint32_t index_size,
                                 uint32_t offset);
void vgpu_virgl_draw_primitive_indexed(vgpu_virgl_encoder *encoder,
                                       uint32_t index_count,
                                       uint32_t primitive_mode,
                                       uint32_t base_vertex);

#endif
