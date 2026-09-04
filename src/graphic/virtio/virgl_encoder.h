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
void vgpu_virgl_create_sampler_state(vgpu_virgl_encoder *encoder,
                                     uint32_t handle);
void vgpu_virgl_create_sampler_view(vgpu_virgl_encoder *encoder,
                                    uint32_t handle, uint32_t resource,
                                    uint32_t format);
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
void vgpu_virgl_create_blend_additive(vgpu_virgl_encoder *encoder,
                                      uint32_t handle);
void vgpu_virgl_create_blend_overlay(vgpu_virgl_encoder *encoder,
                                     uint32_t handle);
void vgpu_virgl_create_dsa(vgpu_virgl_encoder *encoder, uint32_t handle);
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
void vgpu_virgl_draw_primitive(vgpu_virgl_encoder *encoder,
                               uint32_t vertex_count,
                               uint32_t primitive_mode);

#endif
