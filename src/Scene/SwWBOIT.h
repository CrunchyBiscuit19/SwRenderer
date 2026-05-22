#pragma once

#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Resource/SwDescriptor.h>

namespace SwWBOIT {
struct Resources {
    SwPipelinePipeline mWorkPipelinePipeline;
    SwPipelineLayout mWorkPipelineLayout;

    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
};
}  // namespace SwWBOIT