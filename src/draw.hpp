#ifndef DRAW_H
#define DRAW_H

#include "libs/vkh/vkh.h"
#include "libs/quickmath.hpp"
#include "imgui/imgui.h"

#include "globals.hpp"

//----------------------------------------------------------------------------//

#define FRAMES_IN_FLIGHT 2
#define DRAW_NUM_PARTICLES 131072
#define DRAW_DEFAULT_NUM_STARS 40000
#define DRAW_DEFAULT_NUM_DUST 38400

enum ParticleDisplayFlags : uint32
{
	PARTICLE_DISPLAY_STARS     = 1u << 0,
	PARTICLE_DISPLAY_DUST      = 1u << 1,
	PARTICLE_DISPLAY_FILAMENTS = 1u << 2,
	PARTICLE_DISPLAY_H2        = 1u << 3,
};

struct ParticleParamsVertGPU
{
	f32 time;
	f32 timeStepYears;
	uint32 activeParticles;
	uint32 displayFlags;
	uint32 perturbationCount;

	f32 starSize;
	f32 dustSize;
	f32 h2Size;

	f32 h2Dist;
	f32 perturbationDamping;
	f32 viewportHeight;
	f32 verticalFovDegrees;
};

struct ParticleGenParamsGPU
{
	uint32 numStars;
	uint32 numDust;
	uint32 numFilaments;
	uint32 numH2Regions;
	uint32 hasDarkMatter;
	uint32 _padding0;
	uint32 _padding1;
	uint32 _padding2;

	f32 galaxyRad;
	f32 coreRad;
	f32 farFieldRad;
	f32 angleOffset;
	f32 exInner;
	f32 exOuter;

	f32 baseHeight;
	f32 height;

	f32 minTemp;
	f32 maxTemp;
	f32 dustBaseTemp;

	f32 minStarOpacity;
	f32 maxStarOpacity;

	f32 minDustOpacity;
	f32 maxDustOpacity;
};

struct DrawImGuiPushConstants
{
	qm::vec2 scale;
	qm::vec2 translate;
};

struct DrawState
{
	VKHinstance* instance;

	//drawing objects:
	VkFormat depthFormat;
	VkImage finalDepthImage;
	VkImageView finalDepthView;
	VkDeviceMemory finalDepthMemory;

	VkRenderPass finalRenderPass;

	uint32 framebufferCount;
	VkFramebuffer* framebuffers;

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];

	VkSemaphore imageAvailableSemaphores[FRAMES_IN_FLIGHT];
	VkSemaphore renderFinishedSemaphores[3]; // Basically we get 3 swapchain images, so we need 3 semaphores to signal when rendering is finished for each image. It's better to dynamically allocate this based on the actual swapchain image count, but for simplicity I'm just using a fixed size array here.
	VkFence inFlightFences[FRAMES_IN_FLIGHT];

	VkBuffer cameraBuffers[FRAMES_IN_FLIGHT];
	VkDeviceMemory cameraBuffersMemory[FRAMES_IN_FLIGHT];
	VkBuffer cameraStagingBuffer;
	VkDeviceMemory cameraStagingBufferMemory;

	//quad vertex buffers:
	VkBuffer quadVertexBuffer;
	VkDeviceMemory quadVertexBufferMemory;
	VkBuffer quadIndexBuffer;
	VkDeviceMemory quadIndexBufferMemory;

	//grid pipeline objects:
	VKHgraphicsPipeline* gridPipeline;
	VKHdescriptorSets* gridDescriptorSets;

	//particle pipeline objects:
	VKHgraphicsPipeline* particlePipeline;
	VKHdescriptorSets* particleDescriptorSets;

	VkDeviceSize particleBufferSize;
	VkBuffer particleBuffer;
	VkDeviceMemory particleBufferMemory;

	ParticleGenParamsGPU particleGenParams;
	ParticleParamsVertGPU particleVertParams;
	bool particleGenerationDirty;
	bool particleTimePaused;
	bool showGrid;

	VKHgraphicsPipeline* imguiPipeline;
	VKHdescriptorSets* imguiDescriptorSets;
	VkImage imguiFontImage;
	VkImageView imguiFontImageView;
	VkDeviceMemory imguiFontMemory;
	VkSampler imguiFontSampler;
	VkBuffer imguiVertexBuffers[FRAMES_IN_FLIGHT];
	VkDeviceMemory imguiVertexBuffersMemory[FRAMES_IN_FLIGHT];
	VkDeviceSize imguiVertexBufferSizes[FRAMES_IN_FLIGHT];
	VkBuffer imguiIndexBuffers[FRAMES_IN_FLIGHT];
	VkDeviceMemory imguiIndexBuffersMemory[FRAMES_IN_FLIGHT];
	VkDeviceSize imguiIndexBufferSizes[FRAMES_IN_FLIGHT];
	f32 imguiMouseWheel;
	f32 imguiMouseWheelH;
};

//----------------------------------------------------------------------------//

struct Vertex
{
	qm::vec3 pos;
	qm::vec2 texCoord;
};

struct GalaxyParticle
{
	qm::vec2 pos;
	f32 height;
	f32 angle;
	f32 tiltAngle;
	f32 angleVel;
	f32 opacity;
	f32 temp;
	uint32 type;
};

//----------------------------------------------------------------------------//

struct DrawParams
{
	struct
	{
		qm::vec3 pos;
		qm::vec3 up;
		qm::vec3 target;

		float dist;

		float fov;
	} cam;
};

//----------------------------------------------------------------------------//

bool draw_init(DrawState** state);
void draw_quit(DrawState* state);

void draw_imgui_begin_frame(DrawState* state, f32 dt);
void draw_imgui_add_scroll(DrawState* state, f32 x, f32 y);

void draw_render(DrawState* state, DrawParams* params, f32 dt);

uint32 draw_get_galaxy_preset_count(void);
const char* draw_get_galaxy_preset_name(uint32 idx);
bool draw_apply_galaxy_preset(DrawState* state, uint32 idx);
uint32 draw_get_active_particle_count(const DrawState* state);

#endif
