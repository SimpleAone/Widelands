/* Minimal copy of the stable virtio_gpu.library driver-v1 ABI.
 * Keep member order synchronized with include/vgpu/api.h in the driver tree. */
#ifndef WL_GRAPHIC_VIRTIO_DRIVER_API_H
#define WL_GRAPHIC_VIRTIO_DRIVER_API_H

#if defined(__amigaos4__)

#include <exec/interfaces.h>
#include <stdint.h>

#define VGPU_API_VERSION UINT32_C(1)
#define VGPU_CAP_VIRGL UINT32_C(0x00000001)
#define VGPU_CAP_POLLING UINT32_C(0x00000002)
#define VGPU_CAP_MULTI_CLIENT UINT32_C(0x00000004)
#define VGPU_CAP_OFFSCREEN_PRESENT UINT32_C(0x00000008)
#define VGPU_CAP_RELOCATABLE_SUBMIT UINT32_C(0x00000010)
#define VGPU_CAP_RESIDENT_SESSION UINT32_C(0x00000400)
#define VGPU_CAP_RESIDENT_READBACK UINT32_C(0x00000800)
#define VGPU_CAP_DOUBLE_BUFFERED_SCANOUT UINT32_C(0x00001000)

typedef uint32_t vgpu_client_handle;
typedef uint32_t vgpu_context_handle;
typedef uint32_t vgpu_resource_handle;
typedef uint32_t vgpu_fence_handle;
typedef uint32_t vgpu_resident_handle;

#define VGPU_MAX_SUBMIT_RESOURCES UINT32_C(16)
#define VGPU_FENCE_PENDING UINT32_C(0)
#define VGPU_FENCE_SIGNALED UINT32_C(1)
#define VGPU_FENCE_ERROR UINT32_C(2)

#define VGPU_RESOURCE_2D UINT32_C(1)

typedef struct vgpu_resource_desc {
	uint32_t struct_size;
	uint32_t type;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t format;
	uint32_t bind_flags;
	uint32_t backing_size;
	uint32_t target;
	uint32_t array_size;
	uint32_t last_level;
	uint32_t nr_samples;
	uint32_t virgl_flags;
} vgpu_resource_desc;

typedef struct vgpu_capabilities {
	uint32_t struct_size;
	uint32_t api_version;
	uint32_t flags;
	uint32_t max_clients;
	uint32_t max_contexts;
	uint32_t max_resources;
	uint32_t max_fences;
} vgpu_capabilities;

typedef struct vgpu_resource_relocation {
	uint32_t offset;
	uint32_t binding;
} vgpu_resource_relocation;

typedef void (*vgpu_log_fn)(void* context, const char* text);

typedef struct vgpu_submit_desc {
	uint32_t struct_size;
	const void* commands;
	uint32_t command_size;
	const vgpu_resource_handle* resources;
	uint32_t resource_count;
	const vgpu_resource_relocation* relocations;
	uint32_t relocation_count;
	vgpu_log_fn log;
	void* log_context;
	const vgpu_resource_handle* diagnostic_roles;
	uint32_t diagnostic_role_count;
	uint32_t command_repeat_count;
} vgpu_submit_desc;

typedef struct vgpu_driver_iface {
	struct InterfaceData Data;
	uint32_t (*Obtain)(struct vgpu_driver_iface* self);
	uint32_t (*Release)(struct vgpu_driver_iface* self);
	void (*Expunge)(struct vgpu_driver_iface* self);
	struct vgpu_driver_iface* (*Clone)(struct vgpu_driver_iface* self);
	uint32_t (*GetAPIVersion)(struct vgpu_driver_iface* self);
	uint32_t (*GetCapabilities)(struct vgpu_driver_iface* self,
	                            vgpu_capabilities* capabilities);
	uint32_t (*OpenClient)(struct vgpu_driver_iface* self, vgpu_client_handle* client);
	uint32_t (*CloseClient)(struct vgpu_driver_iface* self, vgpu_client_handle client);
	uint32_t (*CreateContext)(struct vgpu_driver_iface* self,
	                          vgpu_client_handle client,
	                          vgpu_context_handle* context);
	uint32_t (*DestroyContext)(struct vgpu_driver_iface* self,
	                           vgpu_client_handle client,
	                           vgpu_context_handle context);
	uint32_t (*CreateResource)(struct vgpu_driver_iface* self,
	                           vgpu_client_handle client,
	                           const vgpu_resource_desc* description,
	                           vgpu_resource_handle* resource);
	uint32_t (*DestroyResource)(struct vgpu_driver_iface* self,
	                            vgpu_client_handle client,
	                            vgpu_resource_handle resource);
	uint32_t (*WriteResource)(struct vgpu_driver_iface* self,
	                          vgpu_client_handle client,
	                          vgpu_resource_handle resource,
	                          uint32_t offset,
	                          const void* source,
	                          uint32_t size);
	uint32_t (*ReadResource)(struct vgpu_driver_iface* self,
	                         vgpu_client_handle client,
	                         vgpu_resource_handle resource,
	                         uint32_t offset,
	                         void* destination,
	                         uint32_t size);
	uint32_t (*GetFenceStatus)(struct vgpu_driver_iface* self,
	                           vgpu_client_handle client,
	                           vgpu_fence_handle fence,
	                           uint32_t* status);
	uint32_t (*WaitFence)(struct vgpu_driver_iface* self,
	                      vgpu_client_handle client,
	                      vgpu_fence_handle fence,
	                      uint32_t timeout_ticks,
	                      uint32_t* status);
	uint32_t (*DestroyFence)(struct vgpu_driver_iface* self,
	                         vgpu_client_handle client,
	                         vgpu_fence_handle fence);
	int32_t (*SubmitVirGL)(struct vgpu_driver_iface* self,
	                       vgpu_client_handle client,
	                       vgpu_context_handle context,
	                       const vgpu_submit_desc* submission,
	                       vgpu_fence_handle* fence);
} vgpu_driver_iface;

typedef struct vgpu_resident_iface {
	struct InterfaceData Data;
	uint32_t (*Obtain)(struct vgpu_resident_iface* self);
	uint32_t (*Release)(struct vgpu_resident_iface* self);
	void (*Expunge)(struct vgpu_resident_iface* self);
	struct vgpu_resident_iface* (*Clone)(struct vgpu_resident_iface* self);
	int32_t (*BeginResident)(struct vgpu_resident_iface* self,
	                         vgpu_client_handle client,
	                         vgpu_context_handle context,
	                         const vgpu_resource_handle* resources,
	                         uint32_t resource_count,
	                         vgpu_log_fn log,
	                         void* log_context,
	                         vgpu_resident_handle* resident);
	int32_t (*SubmitResident)(struct vgpu_resident_iface* self,
	                          vgpu_client_handle client,
	                          vgpu_resident_handle resident,
	                          const vgpu_submit_desc* submission,
	                          vgpu_fence_handle* fence);
	int32_t (*EndResident)(struct vgpu_resident_iface* self,
	                       vgpu_client_handle client,
	                       vgpu_resident_handle resident,
	                       vgpu_log_fn log,
	                       void* log_context);
	int32_t (*ReadbackResident)(struct vgpu_resident_iface* self,
	                            vgpu_client_handle client,
	                            vgpu_resident_handle resident,
	                            vgpu_resource_handle resource,
	                            vgpu_log_fn log,
	                            void* log_context);
	int32_t (*SubmitReadbackResident)(struct vgpu_resident_iface* self,
	                                  vgpu_client_handle client,
	                                  vgpu_resident_handle resident,
	                                  const vgpu_submit_desc* submission,
	                                  vgpu_resource_handle resource,
	                                  vgpu_fence_handle* fence);
	int32_t (*BeginSubmitReadbackResident)(struct vgpu_resident_iface* self,
	                                       vgpu_client_handle client,
	                                       vgpu_context_handle context,
	                                       const vgpu_resource_handle* resources,
	                                       uint32_t resource_count,
	                                       const vgpu_submit_desc* submission,
	                                       vgpu_resource_handle resource,
	                                       vgpu_resident_handle* resident,
	                                       vgpu_fence_handle* fence);
} vgpu_resident_iface;

typedef struct vgpu_scanout_iface {
	struct InterfaceData Data;
	uint32_t (*Obtain)(struct vgpu_scanout_iface* self);
	uint32_t (*Release)(struct vgpu_scanout_iface* self);
	void (*Expunge)(struct vgpu_scanout_iface* self);
	struct vgpu_scanout_iface* (*Clone)(struct vgpu_scanout_iface* self);
	int32_t (*SubmitScanoutResident)(struct vgpu_scanout_iface* self,
	                                 vgpu_client_handle client,
	                                 vgpu_resident_handle resident,
	                                 const vgpu_submit_desc* submission,
	                                 vgpu_resource_handle resource,
	                                 uint32_t scanout_id,
	                                 vgpu_fence_handle* fence);
	int32_t (*DisableScanoutResident)(struct vgpu_scanout_iface* self,
	                                  vgpu_client_handle client,
	                                  vgpu_resident_handle resident,
	                                  uint32_t scanout_id,
	                                  vgpu_log_fn log,
	                                  void* log_context);
} vgpu_scanout_iface;

#endif  // __amigaos4__
#endif  // WL_GRAPHIC_VIRTIO_DRIVER_API_H
