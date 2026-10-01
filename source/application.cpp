#include "application.hpp"
#include "math.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

#include <imgui.h>

namespace application {

using graphics::internal::context;
using math::Mat4;
using math::Vec3;

namespace {

struct Vertex {
	Vec3 position;
	Vec3 color;
};

struct Uniforms {
	Mat4 model;
	Mat4 view;
	Mat4 projection;
	float color[4];
};

struct Buffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	void* data;
};

struct Sphere {
	Vec3 position;
	Vec3 rotation;
	Vec3 scale;
	float color[3];

	bool playing;
	float speed;
	float radius;
	float height;
	int turns;
	float time;

	Uniforms uniforms;
	Buffer uniform_buffer;
	VkDescriptorSet descriptor_set;
};

const int stacks = 8;
const int slices = 10;
const int sphere_count = 3;
const char* sphere_names[sphere_count] = { "Центральная", "Спутник 1", "Спутник 2" };

Sphere spheres[sphere_count] = {
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 1.5f, 1.5f, 1.5f }, { 1.0f, 1.0f, 1.0f }, true, 0.5f, 0.0f, 0.0f, 1, 0.0f },
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0.6f, 0.6f, 0.6f }, { 1.0f, 0.6f, 0.3f }, true, 0.6f, 4.0f, 1.0f, 5, 0.0f },
	{ { 0, 0, 0 }, { 30, 0, 0 }, { 0.4f, 0.8f, 0.4f }, { 0.4f, 0.8f, 1.0f }, true, 0.3f, 6.5f, 1.5f, 3, 2.0f },
};

int selected = 0;
int projection_type = 0;
float fov = 60.0f;
float camera_yaw = 30.0f;
float camera_pitch = 25.0f;
float camera_distance = 14.0f;
double last_time = 0.0;

Buffer vertex_buffer;
Buffer index_buffer;
uint32_t index_count;

VkDescriptorSetLayout set_layout;
VkPipelineLayout pipeline_layout;
VkPipeline pipeline;
VkDescriptorPool descriptor_pool;

Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
	VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
	};

	VmaAllocationCreateInfo allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	};

	Buffer buffer = {};
	VmaAllocationInfo info = {};
	vmaCreateBuffer(context.allocator, &buffer_info, &allocation_info,
	                &buffer.buffer, &buffer.allocation, &info);
	buffer.data = info.pMappedData;
	return buffer;
}

VkShaderModule loadShader(const char* path) {
	std::ifstream file(path, std::ios::binary);
	std::vector<char> code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	if (code.empty()) {
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(code.data()),
	};

	VkShaderModule module = VK_NULL_HANDLE;
	vkCreateShaderModule(context.device, &info, nullptr, &module);
	return module;
}

void createSphereMesh() {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (int i = 0; i <= stacks; ++i) {
		float theta = math::pi * i / stacks;

		for (int j = 0; j <= slices; ++j) {
			float phi = 2.0f * math::pi * j / slices;

			Vec3 p = {
				std::sin(theta) * std::cos(phi),
				std::cos(theta),
				std::sin(theta) * std::sin(phi),
			};
			Vec3 color = { (p.x + 1) / 2, (p.y + 1) / 2, (p.z + 1) / 2 };
			vertices.push_back({ p, color });
		}
	}

	for (int i = 0; i < stacks; ++i) {
		for (int j = 0; j < slices; ++j) {
			uint32_t a = i * (slices + 1) + j;
			uint32_t b = a + slices + 1;

			indices.push_back(a);
			indices.push_back(a + 1);
			indices.push_back(b);

			indices.push_back(a + 1);
			indices.push_back(b + 1);
			indices.push_back(b);
		}
	}

	index_count = uint32_t(indices.size());

	vertex_buffer = createBuffer(vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	index_buffer = createBuffer(indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	std::memcpy(vertex_buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	std::memcpy(index_buffer.data, indices.data(), indices.size() * sizeof(uint32_t));
}

void createDescriptors() {
	VkDescriptorSetLayoutBinding binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &binding,
	};

	vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &set_layout);

	VkDescriptorPoolSize pool_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = sphere_count,
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = sphere_count,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	vkCreateDescriptorPool(context.device, &pool_info, nullptr, &descriptor_pool);

	for (Sphere& sphere : spheres) {
		sphere.uniform_buffer = createBuffer(sizeof(Uniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorSetAllocateInfo allocate_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &set_layout,
		};

		vkAllocateDescriptorSets(context.device, &allocate_info, &sphere.descriptor_set);

		VkDescriptorBufferInfo buffer_info = {
			.buffer = sphere.uniform_buffer.buffer,
			.offset = 0,
			.range = sizeof(Uniforms),
		};

		VkWriteDescriptorSet write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = sphere.descriptor_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &buffer_info,
		};

		vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
	}
}

bool createPipeline() {
	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &set_layout,
	};

	vkCreatePipelineLayout(context.device, &layout_info, nullptr, &pipeline_layout);

	VkShaderModule vertex_shader = loadShader(SHADER_DIR "sphere.vert.spv");
	VkShaderModule fragment_shader = loadShader(SHADER_DIR "sphere.frag.spv");

	if (vertex_shader == VK_NULL_HANDLE || fragment_shader == VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, vertex_shader, nullptr);
		vkDestroyShaderModule(context.device, fragment_shader, nullptr);
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main",
		},
	};

	VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertex_attributes[] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position) },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) },
	};

	VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisample = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};

	VkPipelineColorBlendAttachmentState blend_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo color_blend = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment,
	};

	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states,
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic_state,
		.layout = pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

	VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info,
	                                            nullptr, &pipeline);

	vkDestroyShaderModule(context.device, vertex_shader, nullptr);
	vkDestroyShaderModule(context.device, fragment_shader, nullptr);

	return result == VK_SUCCESS;
}

Vec3 trajectory(const Sphere& s) {
	float t = s.time;
	float k = float(s.turns);
	float r = s.radius + s.height * std::cos(k * t);

	return { r * std::cos(t), s.height * std::sin(k * t), r * std::sin(t) };
}

void drawInterface() {
	ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
	ImGui::Begin("Управление", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

	ImGui::SeparatorText("Камера");
	ImGui::RadioButton("Перспективная", &projection_type, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Ортографическая", &projection_type, 1);
	ImGui::SliderFloat("Угол обзора", &fov, 20.0f, 120.0f);
	ImGui::SliderFloat("Азимут", &camera_yaw, -180.0f, 180.0f);
	ImGui::SliderFloat("Возвышение", &camera_pitch, -89.0f, 89.0f);
	ImGui::SliderFloat("Расстояние", &camera_distance, 3.0f, 40.0f);

	ImGui::SeparatorText("Объект");
	for (int i = 0; i < sphere_count; ++i) {
		ImGui::RadioButton(sphere_names[i], &selected, i);
		ImGui::SameLine();
	}
	ImGui::NewLine();

	Sphere& s = spheres[selected];
	ImGui::DragFloat3("Позиция", &s.position.x, 0.05f);
	ImGui::SliderFloat3("Поворот", &s.rotation.x, -180.0f, 180.0f);
	ImGui::SliderFloat3("Масштаб", &s.scale.x, 0.1f, 3.0f);
	ImGui::ColorEdit3("Цвет", s.color);

	ImGui::SeparatorText("Анимация");
	ImGui::Checkbox("Воспроизведение", &s.playing);
	ImGui::SliderFloat("Скорость", &s.speed, 0.0f, 3.0f);
	ImGui::SliderFloat("Радиус", &s.radius, 0.0f, 10.0f);
	ImGui::SliderFloat("Амплитуда", &s.height, 0.0f, 3.0f);
	ImGui::SliderInt("Витки", &s.turns, 1, 10);

	ImGui::End();
}

}

bool initialize() {
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f);

	createSphereMesh();
	createDescriptors();

	if (!createPipeline()) {
		shutdown();
		return false;
	}

	return true;
}

void shutdown() {
	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyPipeline(context.device, pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
	vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, set_layout, nullptr);

	for (Sphere& sphere : spheres) {
		vmaDestroyBuffer(context.allocator, sphere.uniform_buffer.buffer, sphere.uniform_buffer.allocation);
	}
	vmaDestroyBuffer(context.allocator, vertex_buffer.buffer, vertex_buffer.allocation);
	vmaDestroyBuffer(context.allocator, index_buffer.buffer, index_buffer.allocation);
}

void update(double time) {
	float dt = float(time - last_time);
	last_time = time;

	drawInterface();

	float yaw = math::radians(camera_yaw);
	float pitch = math::radians(camera_pitch);
	Vec3 eye = {
		camera_distance * std::cos(pitch) * std::sin(yaw),
		camera_distance * std::sin(pitch),
		camera_distance * std::cos(pitch) * std::cos(yaw),
	};
	Mat4 view = math::lookAt(eye, { 0, 0, 0 }, { 0, 1, 0 });

	float aspect = float(context.swapchain_extent.width) / float(context.swapchain_extent.height);
	float z_near = 0.1f;
	float z_far = 100.0f;
	Mat4 projection;

	if (projection_type == 0) {
		projection = math::perspective(math::radians(fov), aspect, z_near, z_far);
	} else {
		float h = camera_distance * std::tan(math::radians(fov) / 2);
		float w = h * aspect;
		projection = math::orthographic(-w, w, -h, h, z_near, z_far);
	}

	for (Sphere& s : spheres) {
		if (s.playing) {
			s.time += dt * s.speed;
		}

		Vec3 rotation = {
			math::radians(s.rotation.x),
			math::radians(s.rotation.y),
			math::radians(s.rotation.z),
		};

		s.uniforms.model = math::translate(s.position + trajectory(s)) *
		                   math::rotateEuler(rotation) *
		                   math::rotateY(s.time) *
		                   math::scale(s.scale);
		s.uniforms.view = view;
		s.uniforms.projection = projection;
		s.uniforms.color[0] = s.color[0];
		s.uniforms.color[1] = s.color[1];
		s.uniforms.color[2] = s.color[2];
		s.uniforms.color[3] = 1.0f;
	}
}

void render(const graphics::internal::FrameData& fd) {
	if (fd.command_buffer == VK_NULL_HANDLE) {
		return;
	}

	for (Sphere& s : spheres) {
		std::memcpy(s.uniform_buffer.data, &s.uniforms, sizeof(Uniforms));
	}

	VkCommandBuffer cb = fd.command_buffer;

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(cb, &begin_info);

	VkClearValue clear_values[2] = {};
	clear_values[0].color = { { 0.01f, 0.01f, 0.02f, 1.0f } };
	clear_values[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = context.swapchain_extent },
		.clearValueCount = 2,
		.pClearValues = clear_values,
	};

	vkCmdBeginRenderPass(cb, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {
		.width = float(context.swapchain_extent.width),
		.height = float(context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D scissor = { .extent = context.swapchain_extent };

	vkCmdSetViewport(cb, 0, 1, &viewport);
	vkCmdSetScissor(cb, 0, 1, &scissor);

	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cb, 0, 1, &vertex_buffer.buffer, &offset);
	vkCmdBindIndexBuffer(cb, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	for (Sphere& s : spheres) {
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
		                        0, 1, &s.descriptor_set, 0, nullptr);
		vkCmdDrawIndexed(cb, index_count, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(cb);
	vkEndCommandBuffer(cb);
}

}
