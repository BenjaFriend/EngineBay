#include "pch.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"		// For getting devices/queue/command pools

namespace Fling
{
	namespace GraphicsHelpers
	{
		UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props)
		{
			// #TODO Move this to the Physical device abstraction once we create it
			VkPhysicalDeviceMemoryProperties MemProperties;
			vkGetPhysicalDeviceMemoryProperties(t_PhysicalDevice, &MemProperties);

			for (UINT32 i = 0; i < MemProperties.memoryTypeCount; ++i)
			{
				// Check if this filter bit flag is set and it matches our memory properties
				if ((t_Filter & (1 << i)) && (MemProperties.memoryTypes[i].propertyFlags & t_Props) == t_Props)
				{
					return i;
				}
			}

			F_LOG_FATAL("Failed to find suitable memory type!");
			return 0;
		}

		void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory)
		{
			// Create a buffer
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = t_Size;
			bufferInfo.usage = t_Usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(t_Device, &bufferInfo, nullptr, &t_Buffer) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create buffer!");
			}

			// Get the memory requirements
			VkMemoryRequirements MemRequirments = {};
			vkGetBufferMemoryRequirements(t_Device, t_Buffer, &MemRequirments);

			VkMemoryAllocateInfo AllocInfo = {};
			AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			AllocInfo.allocationSize = MemRequirments.size;
			// Using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT may cause worse perf,
			// we could use explicit flushing with vkFlushMappedMemoryRanges
			AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(t_PhysicalDevice, MemRequirments.memoryTypeBits, t_Properties);

			// Allocate the vertex buffer memory
			// #TODO Don't call vkAllocateMemory every time, we should use a custom allocator or
			// VulkanMemoryAllocator library
			if (vkAllocateMemory(t_Device, &AllocInfo, nullptr, &t_BuffMemory) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to alocate buffer memory!");
			}
			vkBindBufferMemory(t_Device, t_Buffer, t_BuffMemory, 0);
		}

		VkCommandBuffer BeginSingleTimeCommands()
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();
			const VkCommandPool& CommandPool = Renderer::Get().GetCommandPool();

			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = CommandPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			vkAllocateCommandBuffers(Device, &allocInfo, &commandBuffer);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffer, &beginInfo);
			return commandBuffer;
		}

		void EndSingleTimeCommands(VkCommandBuffer t_CommandBuffer)
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();
			VkCommandPool CmdPool = Renderer::Get().GetCommandPool();
			VkQueue GraphicsQueue = Renderer::Get().GetGraphicsQueue();

			vkEndCommandBuffer(t_CommandBuffer);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &t_CommandBuffer;

			vkQueueSubmit(GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(GraphicsQueue);

			vkFreeCommandBuffers(Device, CmdPool, 1, &t_CommandBuffer);
		}

		void CreateVkImage(
			UINT32 t_Width,
			UINT32 t_Height,
			VkFormat t_Format, 
			VkImageTiling t_Tiling, 
			VkImageUsageFlags t_Useage, 
			VkMemoryPropertyFlags t_Props, 
			VkImage& t_Image,
			VkDeviceMemory& t_Memory
		)
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();
			VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();

			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = t_Width;
			imageInfo.extent.height = t_Height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;

			imageInfo.format = t_Format;
			imageInfo.tiling = t_Tiling;

			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = t_Useage;

			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateImage(Device, &imageInfo, nullptr, &t_Image) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create image!");
			}

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(Device, t_Image, &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(PhysDevice, memRequirements.memoryTypeBits, t_Props);

			if (vkAllocateMemory(Device, &allocInfo, nullptr, &t_Memory) != VK_SUCCESS) 
			{
				F_LOG_FATAL("Failed to allocate image memory!");
			}

			vkBindImageMemory(Device, t_Image, t_Memory, 0);
		}

		void CreateVkSampler(VkFilter t_magFilter, VkFilter t_minFilter, VkSamplerMipmapMode t_mipmapMode, VkSamplerAddressMode t_addressModeU, VkSamplerAddressMode t_addressModeV, VkSamplerAddressMode t_addressModeM, VkBorderColor t_borderColor, VkSampler& t_sampler)
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();

			VkSamplerCreateInfo samplerInfo = {};
			samplerInfo.magFilter = t_magFilter;
			samplerInfo.minFilter = t_minFilter;
			samplerInfo.mipmapMode = t_mipmapMode;
			samplerInfo.addressModeU = t_addressModeU;
			samplerInfo.addressModeV = t_addressModeV;
			samplerInfo.addressModeW = t_addressModeM;
			samplerInfo.borderColor = t_borderColor;
			

			if (vkCreateSampler(Device, &samplerInfo, nullptr, &t_sampler) != VK_SUCCESS)
			{
				F_LOG_ERROR("Failed to create sampler");
			}
		}

		VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType t_type, UINT32 t_descriptorCount)
		{
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = t_type;
			descriptorPoolSize.descriptorCount = t_descriptorCount;
			return descriptorPoolSize;
		}

		VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
			const std::vector<VkDescriptorPoolSize>& t_poolSizes, 
			UINT32 t_maxSets)
		{
			VkDescriptorPoolCreateInfo descriptorPoolInfo{};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(t_poolSizes.size());
			descriptorPoolInfo.pPoolSizes = t_poolSizes.data();
			descriptorPoolInfo.maxSets = t_maxSets;
			return descriptorPoolInfo;
		}

		VkDescriptorSetLayoutBinding DescriptorSetLayoutBindings(VkDescriptorType t_type, VkShaderStageFlags t_stageFlags, uint32_t t_binding, uint32_t t_descriptorCount)
		{
			VkDescriptorSetLayoutBinding setLayoutBinding = {};
			setLayoutBinding.descriptorType = t_type;
			setLayoutBinding.stageFlags = t_stageFlags;
			setLayoutBinding.binding = t_binding;
			setLayoutBinding.descriptorCount = t_descriptorCount;
			return setLayoutBinding;
		}

		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding>& t_bindings)
		{
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.pBindings = t_bindings.data();
			descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(t_bindings.size());
			return descriptorSetLayoutCreateInfo;
		}

		VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool t_descriptorPool, const VkDescriptorSetLayout * t_pSetLayouts, UINT32 t_descriptorSetCount)
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
			descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocateInfo.descriptorPool = t_descriptorPool;
			descriptorSetAllocateInfo.pSetLayouts = t_pSetLayouts;
			descriptorSetAllocateInfo.descriptorSetCount = t_descriptorSetCount;
			return descriptorSetAllocateInfo;
		}

		VkDescriptorImageInfo DescriptorImageInfo(VkSampler t_sampler, VkImageView t_imageView, VkImageLayout t_imageLayout)
		{
			VkDescriptorImageInfo descriptorImageInfo{};
			descriptorImageInfo.sampler = t_sampler;
			descriptorImageInfo.imageView = t_imageView;
			descriptorImageInfo.imageLayout = t_imageLayout;
			return descriptorImageInfo;
		}

		VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet t_dstSet, VkDescriptorType t_type, UINT32 t_binding, VkDescriptorImageInfo * imageInfo, UINT32 descriptorCount)
		{
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = t_dstSet;
			writeDescriptorSet.descriptorType = t_type;
			writeDescriptorSet.dstBinding = t_binding;
			writeDescriptorSet.pImageInfo = imageInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

		VkPushConstantRange PushConstantRange(VkShaderStageFlags t_stageFlags, UINT32 t_size, UINT32 t_offset)
		{
			VkPushConstantRange pushConstantRange{};
			pushConstantRange.stageFlags = t_stageFlags;
			pushConstantRange.offset = t_offset;
			pushConstantRange.size = t_size;
			return pushConstantRange;
		}

		VkPipelineLayoutCreateInfo PiplineLayoutCreateInfo(const VkDescriptorSetLayout * t_pSetLayouts, UINT32 t_setLayoutCount)
		{
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = t_setLayoutCount;
			pipelineLayoutCreateInfo.pSetLayouts = t_pSetLayouts;
			return pipelineLayoutCreateInfo;
		}

		VkImageView CreateVkImageView(VkImage t_Image, VkFormat t_Format, VkImageAspectFlags t_AspectFalgs)
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();

			assert(Device != VK_NULL_HANDLE);

			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = t_Image;

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // use 3D for cube maps
			createInfo.format = t_Format;

			createInfo.subresourceRange.aspectMask = t_AspectFalgs;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkImageView imageView = VK_NULL_HANDLE;
			if (vkCreateImageView(Device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
			{
				F_LOG_ERROR("Failed to create image views!");
			}
			return imageView;
		}

		VkFormat FindSupportedFormat(const std::vector<VkFormat>& t_Candidates, VkImageTiling t_Tiling, VkFormatFeatureFlags t_Features)
		{
			VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();
			for (VkFormat CurFormat : t_Candidates)
			{
				VkFormatProperties Props;
				vkGetPhysicalDeviceFormatProperties(PhysDevice, CurFormat, &Props);

				if (t_Tiling == VK_IMAGE_TILING_LINEAR && (Props.linearTilingFeatures & t_Features) == t_Features)
				{
					return CurFormat;
				}
				else if (t_Tiling == VK_IMAGE_TILING_OPTIMAL && (Props.optimalTilingFeatures & t_Features) == t_Features)
				{
					return CurFormat;
				}
			}
			// Ruh ro
			F_LOG_ERROR("Failed to find supported format! Returning VK_FORMAT_D32_SFLOAT by default");
			return VK_FORMAT_D32_SFLOAT;
		}

		void TransitionImageLayout(VkImage t_Image, VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout)
		{
			VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = t_oldLayout;
			barrier.newLayout = t_NewLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = t_Image;

			// Make sure that we use the correct aspect bit depending on if we are for the depth buffer or not
			if (t_NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (GraphicsHelpers::HasStencilComponent(t_Format)) 
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else 
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			// Handle transition barrier masks
			VkPipelineStageFlags SourceStage = 0;
			VkPipelineStageFlags DestinationStage = 0;

			if (t_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && t_NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (t_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && t_NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else if (t_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && t_NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				DestinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			}
			else 
			{
				F_LOG_ERROR("Unsupported layout transition!");
			}

			vkCmdPipelineBarrier(
				commandBuffer,
				SourceStage, DestinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
		}

		bool HasStencilComponent(VkFormat t_format)
		{
			return t_format == VK_FORMAT_D32_SFLOAT_S8_UINT || t_format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

}	// namespace GraphicsHelpers
}   // namespace Fling