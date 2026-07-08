#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Scene/SwSystem.h>

#include <filesystem>

namespace SwWBOIT {

constexpr vk::ClearColorValue RVL_CLEAR_VALUE{1.f, 0.f, 0.f, 0.f};
constexpr vk::Format RVL_FORMAT{vk::Format::eR16Sfloat};
static const std::filesystem::path WBOIT_SHADERS_DIR{std::filesystem::path(SHADERS_DIR) / "WBOIT"};
static const std::filesystem::path WBOIT_VERTEX_SHADER_PATH{WBOIT_SHADERS_DIR / "SwWBOITComposite.vert.spv"};
static const std::filesystem::path WBOIT_FRAGMENT_SHADER_PATH{WBOIT_SHADERS_DIR / "SwWBOITComposite.frag.spv"};

struct Resources {
    SwColorImage2D mAccumImage;
    SwColorImage2D mRvlImage;

    SwGraphicsPipelineBundle mWorkPipelineBundle;
    SwPipelineLayout mWorkPipelineLayout;

    SwDescriptorSet mWorkDescriptorSet;
    SwDescriptorLayout mWorkDescriptorLayout;
};
class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
};
}  // namespace SwWBOIT