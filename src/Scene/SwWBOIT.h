#pragma once

#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwDescriptor.h>

namespace SwWBOIT {
struct Resources {
    SwColorImage2D mAccumImage;
    SwColorImage2D mRevealageImage;

    SwPipelinePipeline mPipelinePipeline;
    SwPipelineLayout mPipelineLayout;

    SwDescriptorSet mDescriptorSet;
    SwDescriptorLayout mDescriptorSetLayout;
};
}  // namespace SwWBOIT