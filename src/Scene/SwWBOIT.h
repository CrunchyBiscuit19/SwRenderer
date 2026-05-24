#pragma once

#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwDescriptor.h>

namespace SwWBOIT {

constexpr vk::ClearColorValue RVL_CLEAR_VALUE{1.f, 0.f, 0.f, 0.f};    
constexpr vk::Format RVL_FORMAT{vk::Format::eR16Sfloat};

struct Resources {
    SwColorImage2D mAccumImage;
    SwColorImage2D mRvlImage;

    SwPipelinePipeline mWorkPipelinePipeline;
    SwPipelineLayout mWorkPipelineLayout;

    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
};
}  // namespace SwWBOIT