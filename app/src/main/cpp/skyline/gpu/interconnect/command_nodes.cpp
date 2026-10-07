// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "command_nodes.h"
#include <gpu/resolution_blit.h>
#include "gpu/texture/texture.h"
#include <vulkan/vulkan_enums.hpp>

namespace skyline::gpu::interconnect::node {
    RenderPassNode::RenderPassNode(vk::Rect2D renderArea) : renderArea{renderArea} {}

    u32 RenderPassNode::AddAttachment(TextureView *view, GPU &gpu) {
        auto vkView{view->GetView()};
        auto attachment{std::find(attachments.begin(), attachments.end(), vkView)};
        if (attachment == attachments.end()) {
            // If we cannot find any matches for the specified attachment, we add it as a new one
            attachments.push_back(vkView);
            if (!gpu.resolutionScale.IsNative()) attachmentViews.push_back(view->shared_from_this());

            if (gpu.traits.supportsImagelessFramebuffers)
                attachmentInfo.push_back(vk::FramebufferAttachmentImageInfo{
                    .flags = view->texture->flags,
                    .usage = view->texture->usage,
                    .width = view->texture->dimensions.width,
                    .height = view->texture->dimensions.height,
                    .layerCount = view->range.layerCount,
                    .viewFormatCount = 1,
                    .pViewFormats = &view->format->vkFormat,
                });

            attachmentDescriptions.push_back(vk::AttachmentDescription{
                .format = *view->format,
                .initialLayout = view->texture->layout,
                .finalLayout = view->texture->layout,
                .flags = vk::AttachmentDescriptionFlagBits::eMayAlias
            });

            if (auto usage{view->texture->GetLastRenderPassUsage()}; usage != texture::RenderPassUsage::None) {
                vk::PipelineStageFlags attachmentDstStageMask{};
                if (view->format->vkAspect & vk::ImageAspectFlagBits::eColor)
                    attachmentDstStageMask |= vk::PipelineStageFlagBits::eColorAttachmentOutput;
                else if (view->format->vkAspect & (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
                    attachmentDstStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;

                dependencyDstStageMask |= attachmentDstStageMask;

                if (usage == texture::RenderPassUsage::RenderTarget)
                    dependencySrcStageMask |= attachmentDstStageMask;
                else if (usage == texture::RenderPassUsage::Sampled)
                    dependencySrcStageMask |= view->texture->GetReadStageMask();
            }

            return static_cast<u32>(attachments.size() - 1);
        } else {
            // If we've got a match from a previous subpass, we need to preserve the attachment till the current subpass
            auto attachmentIndex{static_cast<u32>(std::distance(attachments.begin(), attachment))};

            auto it{subpassDescriptions.begin()};
            auto getSubpassAttachmentRange{[this](const vk::SubpassDescription &subpassDescription) {
                // Find the bounds for the attachment references belonging to the current subpass
                auto referenceBeginIt{attachmentReferences.begin()};
                referenceBeginIt += reinterpret_cast<uintptr_t>(subpassDescription.pInputAttachments) / sizeof(vk::AttachmentReference);

                auto referenceEndIt{referenceBeginIt + subpassDescription.inputAttachmentCount + subpassDescription.colorAttachmentCount}; // We depend on all attachments being contiguous for a subpass, this will horribly break if that assumption is broken
                if (reinterpret_cast<uintptr_t>(subpassDescription.pDepthStencilAttachment) != NoDepthStencil)
                    referenceEndIt++;

                return std::make_pair(referenceBeginIt, referenceEndIt);
            }};

            // We want to find the first subpass that utilizes the attachment we want to preserve
            for (; it != subpassDescriptions.end(); it++) {
                auto[attachmentReferenceBegin, attachmentReferenceEnd]{getSubpassAttachmentRange(*it)};

                // Iterate over all attachment references in the current subpass to see if they point to our target attachment
                if (std::find_if(attachmentReferenceBegin, attachmentReferenceEnd, [&](const vk::AttachmentReference &reference) {
                    return reference.attachment == attachmentIndex;
                }) != attachmentReferenceEnd)
                    break;
            }

            if (it == subpassDescriptions.end())
                // We assume an attachment is used by the latest subpass if it is not utilized by any past subpasses
                return attachmentIndex;

            // We want to preserve the attachment for all subpasses till the current subpass
            auto lastUsageIt{it}; //!< The last subpass that the attachment has been used in for creating a dependency
            for (; it != subpassDescriptions.end(); it++) {
                auto[attachmentReferenceBegin, attachmentReferenceEnd]{getSubpassAttachmentRange(*it)};

                if (std::find_if(attachmentReferenceBegin, attachmentReferenceEnd, [&](const vk::AttachmentReference &reference) {
                    return reference.attachment == attachmentIndex;
                }) != attachmentReferenceEnd) {
                    lastUsageIt = it;
                    continue; // If a subpass uses an attachment then it doesn't need to be preserved
                }

                auto &subpassPreserveAttachments{preserveAttachmentReferences[static_cast<size_t>(std::distance(subpassDescriptions.begin(), it))]};
                if (std::find(subpassPreserveAttachments.begin(), subpassPreserveAttachments.end(), attachmentIndex) != subpassPreserveAttachments.end())
                    subpassPreserveAttachments.push_back(attachmentIndex);
            }

            // We want to ensure writes to the attachment from the last subpass using it are complete prior to using it in the latest subpass
            vk::SubpassDependency dependency{
                .srcSubpass = static_cast<u32>(std::distance(subpassDescriptions.begin(), lastUsageIt)),
                .dstSubpass = static_cast<uint32_t>(subpassDescriptions.size()), // We assume that the next subpass is using the attachment
                .dstStageMask = vk::PipelineStageFlagBits::eAllGraphics,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion,
            };
            if (view->format->vkAspect & vk::ImageAspectFlagBits::eColor) {
                dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            } else if (view->format->vkAspect & (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)) {
                dependency.srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
                dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            }

            if (std::find(subpassDependencies.begin(), subpassDependencies.end(), dependency) == subpassDependencies.end())
                subpassDependencies.push_back(dependency);

            return attachmentIndex;
        }
    }

    void RenderPassNode::AddSubpass(span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment, GPU &gpu) {
        attachmentReferences.reserve(attachmentReferences.size() + inputAttachments.size() + colorAttachments.size() + (depthStencilAttachment ? 1 : 0));

        auto inputAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
        for (auto &attachment : inputAttachments) {
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = AddAttachment(attachment, gpu),
                .layout = attachment->texture->layout,
            });
        }

        auto colorAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)}; // Calculate new base offset as it has changed since we pushed the input attachments
        for (auto &attachment : colorAttachments) {
            if (attachment)
                attachmentReferences.push_back(vk::AttachmentReference{
                    .attachment = AddAttachment(attachment, gpu),
                    .layout = attachment->texture->layout,
                });
            else
                attachmentReferences.push_back(vk::AttachmentReference{
                    .attachment = VK_ATTACHMENT_UNUSED,
                    .layout = vk::ImageLayout::eUndefined,
                });
        }

        auto depthStencilAttachmentOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
        if (depthStencilAttachment) {
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = AddAttachment(depthStencilAttachment, gpu),
                .layout = depthStencilAttachment->texture->layout,
            });
        }

        preserveAttachmentReferences.emplace_back(); // We need to create storage for any attachments that might need to preserved by this pass

        // Note: We encode the offsets as the pointers due to vector pointer invalidation, RebasePointer(...) can be utilized to deduce the real pointer
        subpassDescriptions.push_back(vk::SubpassDescription{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = static_cast<u32>(inputAttachments.size()),
            .pInputAttachments = reinterpret_cast<vk::AttachmentReference *>(inputAttachmentsOffset),
            .colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
            .pColorAttachments = reinterpret_cast<vk::AttachmentReference *>(colorAttachmentsOffset),
            .pDepthStencilAttachment = reinterpret_cast<vk::AttachmentReference *>(depthStencilAttachment ? depthStencilAttachmentOffset : NoDepthStencil),
        });
    }

    void RenderPassNode::UpdateDependency(vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask) {
        dependencySrcStageMask |= srcStageMask;
        dependencyDstStageMask |= dstStageMask;
    }

    bool RenderPassNode::ClearColorAttachment(u32 colorAttachment, const vk::ClearColorValue &value, GPU& gpu) {
        auto attachmentReference{RebasePointer(attachmentReferences, subpassDescriptions.back().pColorAttachments) + colorAttachment};
        auto attachmentIndex{attachmentReference->attachment};

        for (const auto &reference : attachmentReferences)
            if (reference.attachment == attachmentIndex && &reference != attachmentReference)
                return false;

        auto &attachmentDescription{attachmentDescriptions.at(attachmentIndex)};
        if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eLoad) {
            attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;

            clearValues.resize(attachmentIndex + 1);
            clearValues[attachmentIndex].color = value;

            return true;
        } else if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eClear && clearValues[attachmentIndex].color.uint32 == value.uint32) {
            return true;
        }

        return false;
    }

    bool RenderPassNode::ClearDepthStencilAttachment(const vk::ClearDepthStencilValue &value, GPU& gpu) {
        auto attachmentReference{RebasePointer(attachmentReferences, subpassDescriptions.back().pDepthStencilAttachment)};
        auto attachmentIndex{attachmentReference->attachment};

        for (const auto &reference : attachmentReferences)
            if (reference.attachment == attachmentIndex && &reference != attachmentReference)
                return false;

        auto &attachmentDescription{attachmentDescriptions.at(attachmentIndex)};
        if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eLoad) {
            attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;

            clearValues.resize(attachmentIndex + 1);
            clearValues[attachmentIndex].depthStencil = value;

            return true;
        } else if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eClear && clearValues[attachmentIndex].depthStencil == value) {
            return true;
        }

        return false;
    }

    static void ReportResolutionPass(GPU &gpu, size_t reason) {
        auto count{++gpu.resolutionPassCounts[reason]};
        if (count == 1) {
            constexpr std::array<const char *, 5> reasons{
                "native: shader, helper or attachment feedback",
                "native: unsupported attachment or partial render area",
                "native: device extent or blit support limit",
                "native: optional allocation unavailable",
                "scaled graphics pass"};
            LOGI("Resolution scale {}x: {}", gpu.resolutionScale.Factor(), reasons[reason]);
        }
    }

    bool RenderPassNode::PrepareResolution(GPU &gpu) {
        if (gpu.resolutionScale.IsNative()) return false;
        if (!allowResolutionScaling || gpu.resolutionQueriesSeen.load()) {
            ReportResolutionPass(gpu, 0);
            return false;
        }
        if (gpu.resolutionAllocationFailed) return false;
        if (subpassDescriptions.size() != 1 || subpassDescriptions[0].inputAttachmentCount || attachmentViews.empty()) {
            ReportResolutionPass(gpu, 1);
            return false;
        }
        const auto limits{gpu.vkPhysicalDevice.getProperties().limits};
        for (size_t i{}; i < attachmentViews.size(); i++) {
            const auto view{attachmentViews[i]};
            const auto &texture{*view->texture};
            const auto dimensions{texture.dimensions};
            // Until shader coordinate compensation and mixed attachment sizes
            // are implemented, only complete, ordinary 2D surfaces are eligible.
            if (view->type != vk::ImageViewType::e2D || view->range.baseMipLevel ||
                view->range.levelCount != 1 || view->range.baseArrayLayer || view->range.layerCount != 1 ||
                dimensions.depth != 1 || texture.layerCount != 1 || texture.sampleCount != vk::SampleCountFlagBits::e1 ||
                view->format != texture.format || view->range.aspectMask != texture.format->vkAspect ||
                texture.layout != vk::ImageLayout::eGeneral || renderArea.offset != vk::Offset2D{} ||
                renderArea.extent != vk::Extent2D{dimensions.width, dimensions.height}) {
                ReportResolutionPass(gpu, 1);
                return false;
            }
            for (size_t j{}; j < i; j++) {
                if (attachmentViews[j]->texture == view->texture) {
                    ReportResolutionPass(gpu, 1);
                    return false;
                }
            }
            const auto extent{gpu.resolutionScale.TryExtent({dimensions.width, dimensions.height},
                {std::min(limits.maxImageDimension2D, std::min(limits.maxFramebufferWidth, limits.maxViewportDimensions[0])),
                 std::min(limits.maxImageDimension2D, std::min(limits.maxFramebufferHeight, limits.maxViewportDimensions[1]))})};
            const auto features{gpu.vkPhysicalDevice.getFormatProperties(texture.format->vkFormat).optimalTilingFeatures};
            const auto required{vk::FormatFeatureFlagBits::eBlitSrc | vk::FormatFeatureFlagBits::eBlitDst};
            if (!extent || (features & required) != required) {
                ReportResolutionPass(gpu, 2);
                return false;
            }
        }
        try {
            for (auto view : attachmentViews) {
                auto &texture{*view->texture};
                const auto extent{*gpu.resolutionScale.TryExtent({texture.dimensions.width, texture.dimensions.height},
                    {limits.maxImageDimension2D, limits.maxImageDimension2D})};
                const vk::Extent2D scaledExtent{extent.width, extent.height};
                if (!texture.scaledRenderTarget) {
                    // Reserve a conservative padded estimate before optional allocation.
                    const auto bytes{texture.format->GetSize((extent.width + 127u) & ~127u,
                        (extent.height + 63u) & ~63u) + 256u * 1024u};
                    auto reservation{std::make_shared<resolution::Reservation>(gpu.resolutionAllocatedBytes, bytes)};
                    if (!reservation->reserved) {
                        scaledTargets.clear();
                        ReportResolutionPass(gpu, 3);
                        return false;
                    }
                    const auto usage{vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                        (view->range.aspectMask & vk::ImageAspectFlagBits::eColor ? vk::ImageUsageFlagBits::eColorAttachment : vk::ImageUsageFlagBits::eDepthStencilAttachment)};
                    auto image{gpu.memory.AllocateImage(vk::ImageCreateInfo{
                        .imageType = vk::ImageType::e2D,
                        .format = texture.format->vkFormat,
                        .extent = {extent.width, extent.height, 1},
                        .mipLevels = 1,
                        .arrayLayers = 1,
                        .samples = vk::SampleCountFlagBits::e1,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = usage,
                        .sharingMode = vk::SharingMode::eExclusive,
                        .initialLayout = vk::ImageLayout::eUndefined,
                    })};
                    auto imageView{gpu.vkDevice.createImageView(vk::ImageViewCreateInfo{
                        .image = image.vkImage,
                        .viewType = vk::ImageViewType::e2D,
                        .format = texture.format->vkFormat,
                        .subresourceRange = view->range,
                    })};
                    texture.scaledRenderTarget = std::make_shared<ScaledRenderTarget>(std::move(reservation), std::move(image), std::move(imageView), scaledExtent);
                    if (++gpu.resolutionAttachmentCount <= 8) LOGI("Scaled render attachment: {}x{} -> {}x{} ({})", texture.dimensions.width, texture.dimensions.height,
                        extent.width, extent.height, vk::to_string(texture.format->vkFormat));
                }
                scaledTargets.push_back(texture.scaledRenderTarget);
            }
        } catch (const vk::SystemError &error) {
            const auto result{static_cast<vk::Result>(error.code().value())};
            if (result != vk::Result::eErrorOutOfDeviceMemory && result != vk::Result::eErrorOutOfHostMemory &&
                result != vk::Result::eErrorFormatNotSupported)
                throw; // Device loss and unrelated errors must never be disguised.
            gpu.resolutionAllocationFailed = true;
            LOGW("Resolution scaling disabled for this session: {}", error.what());
            scaledTargets.clear();
            for (auto view : attachmentViews) view->texture->scaledRenderTarget.reset();
            ReportResolutionPass(gpu, 3);
            return false;
        } catch (const std::bad_alloc &) {
            gpu.resolutionAllocationFailed = true;
            scaledTargets.clear();
            for (auto view : attachmentViews) view->texture->scaledRenderTarget.reset();
            ReportResolutionPass(gpu, 3);
            return false;
        }
        return true;
    }

    static vk::Filter ResolutionFilter(const std::shared_ptr<TextureView> &view, GPU &gpu) {
        const auto features{gpu.vkPhysicalDevice.getFormatProperties(view->texture->format->vkFormat).optimalTilingFeatures};
        return (view->range.aspectMask & vk::ImageAspectFlagBits::eColor) &&
            (features & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) ? vk::Filter::eLinear : vk::Filter::eNearest;
    }

    void RenderPassNode::ResolveResolution(vk::raii::CommandBuffer &commandBuffer, GPU &gpu) {
        for (size_t i{}; i < scaledTargets.size(); i++) {
            const auto view{attachmentViews[i]};
            const auto &target{scaledTargets[i]};
            const vk::Extent2D nativeExtent{view->texture->dimensions.width, view->texture->dimensions.height};
            resolution::Barrier(commandBuffer, target->image.vkImage, view->range.aspectMask);
            resolution::Barrier(commandBuffer, view->texture->GetBacking(), view->range.aspectMask);
            resolution::Blit(commandBuffer, target->image.vkImage, target->extent,
                view->texture->GetBacking(), nativeExtent, view->range.aspectMask, ResolutionFilter(view, gpu));
            resolution::Barrier(commandBuffer, view->texture->GetBacking(), view->range.aspectMask);
        }
    }

    vk::RenderPass RenderPassNode::operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
        auto preserveAttachmentIt{preserveAttachmentReferences.begin()};
        for (auto &subpassDescription : subpassDescriptions) {
            subpassDescription.pInputAttachments = RebasePointer(attachmentReferences, subpassDescription.pInputAttachments);
            subpassDescription.pColorAttachments = RebasePointer(attachmentReferences, subpassDescription.pColorAttachments);

            auto depthStencilAttachmentOffset{reinterpret_cast<uintptr_t>(subpassDescription.pDepthStencilAttachment)};
            if (depthStencilAttachmentOffset != NoDepthStencil)
                subpassDescription.pDepthStencilAttachment = RebasePointer(attachmentReferences, subpassDescription.pDepthStencilAttachment);
            else
                subpassDescription.pDepthStencilAttachment = nullptr;

            subpassDescription.preserveAttachmentCount = static_cast<u32>(preserveAttachmentIt->size());
            subpassDescription.pPreserveAttachments = preserveAttachmentIt->data();
            preserveAttachmentIt++;
        }

        if (dependencyDstStageMask && dependencySrcStageMask) {
            commandBuffer.pipelineBarrier(dependencySrcStageMask, dependencyDstStageMask, {}, {vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
            }}, {}, {});
        }

        auto renderPass{gpu.renderPassCache.GetRenderPass(vk::RenderPassCreateInfo{
            .attachmentCount = static_cast<u32>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = static_cast<u32>(subpassDescriptions.size()),
            .pSubpasses = subpassDescriptions.data(),
            .dependencyCount = static_cast<u32>(subpassDependencies.size()),
            .pDependencies = subpassDependencies.data(),
        })};

        const bool scaled{PrepareResolution(gpu)};
        if (scaled) {
            const auto extent{scaledTargets.front()->extent};
            std::vector<vk::ImageView> scaledViews;
            for (const auto &target : scaledTargets) scaledViews.push_back(*target->view);
            // Do not put transient views in the global framebuffer cache. Both
            // the framebuffer and images are retained until the fence completes.
            std::shared_ptr<vk::raii::Framebuffer> framebuffer;
            try {
                framebuffer = std::make_shared<vk::raii::Framebuffer>(gpu.vkDevice, vk::FramebufferCreateInfo{
                    .renderPass = renderPass,
                    .attachmentCount = static_cast<u32>(scaledViews.size()),
                    .pAttachments = scaledViews.data(),
                    .width = extent.width,
                    .height = extent.height,
                    .layers = 1,
                });
            } catch (const vk::SystemError &error) {
                const auto result{static_cast<vk::Result>(error.code().value())};
                if (result != vk::Result::eErrorOutOfDeviceMemory && result != vk::Result::eErrorOutOfHostMemory) throw;
                gpu.resolutionAllocationFailed = true;
                scaledTargets.clear();
                for (auto view : attachmentViews) view->texture->scaledRenderTarget.reset();
                ReportResolutionPass(gpu, 3);
            } catch (const std::bad_alloc &) {
                gpu.resolutionAllocationFailed = true;
                scaledTargets.clear();
                for (auto view : attachmentViews) view->texture->scaledRenderTarget.reset();
                ReportResolutionPass(gpu, 3);
            }
            if (framebuffer) {
                cycle->AttachObject(framebuffer);
                for (size_t i{}; i < scaledTargets.size(); i++) {
                    auto view{attachmentViews[i]};
                    const auto &target{scaledTargets[i]};
                    cycle->AttachObject(target);
                    cycle->AttachObject(view);
                    resolution::Barrier(commandBuffer, view->texture->GetBacking(), view->range.aspectMask);
                    resolution::Barrier(commandBuffer, target->image.vkImage, view->range.aspectMask, !target->initialized);
                    target->initialized = true;
                    resolution::Blit(commandBuffer, view->texture->GetBacking(), renderArea.extent,
                        target->image.vkImage, extent, view->range.aspectMask, ResolutionFilter(view, gpu));
                    resolution::Barrier(commandBuffer, target->image.vkImage, view->range.aspectMask);
                }
                renderResolutionState.Begin(commandBuffer, float(extent.width) / renderArea.extent.width,
                    float(extent.height) / renderArea.extent.height, extent);
                commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{
                    .renderPass = renderPass,
                    .framebuffer = **framebuffer,
                    .renderArea = {{0, 0}, extent},
                    .clearValueCount = static_cast<u32>(clearValues.size()),
                    .pClearValues = clearValues.data(),
                }, vk::SubpassContents::eInline);
                ReportResolutionPass(gpu, 4);
                return renderPass;
            }
        }
        renderResolutionState.Begin(commandBuffer, 1.0f, 1.0f, renderArea.extent);
        auto useImagelessFramebuffer{gpu.traits.supportsImagelessFramebuffers};
        cache::FramebufferCreateInfo framebufferCreateInfo{
            vk::FramebufferCreateInfo{
                .flags = useImagelessFramebuffer ? vk::FramebufferCreateFlagBits::eImageless : vk::FramebufferCreateFlags{},
                .renderPass = renderPass,
                .attachmentCount = static_cast<u32>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = renderArea.extent.width + static_cast<u32>(renderArea.offset.x),
                .height = renderArea.extent.height + static_cast<u32>(renderArea.offset.y),
                .layers = 1,
            },
            vk::FramebufferAttachmentsCreateInfo{
                .attachmentImageInfoCount = static_cast<u32>(attachmentInfo.size()),
                .pAttachmentImageInfos = attachmentInfo.data(),
            }
        };

        if (!useImagelessFramebuffer)
            framebufferCreateInfo.unlink<vk::FramebufferAttachmentsCreateInfo>();

        auto framebuffer{gpu.framebufferCache.GetFramebuffer(framebufferCreateInfo)};

        vk::StructureChain<vk::RenderPassBeginInfo, vk::RenderPassAttachmentBeginInfo> renderPassBeginInfo{
            vk::RenderPassBeginInfo{
                .renderPass = renderPass,
                .framebuffer = framebuffer,
                .renderArea = renderArea,
                .clearValueCount = static_cast<u32>(clearValues.size()),
                .pClearValues = clearValues.data(),
            },
            vk::RenderPassAttachmentBeginInfo{
                .attachmentCount = static_cast<u32>(attachments.size()),
                .pAttachments = attachments.data(),
            }
        };

        if (!useImagelessFramebuffer)
            renderPassBeginInfo.unlink<vk::RenderPassAttachmentBeginInfo>();

        commandBuffer.beginRenderPass(renderPassBeginInfo.get<vk::RenderPassBeginInfo>(), vk::SubpassContents::eInline);

        return renderPass;
    }
}

