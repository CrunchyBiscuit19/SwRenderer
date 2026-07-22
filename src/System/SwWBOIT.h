#pragma once

#include <Resource/SwDescriptor.h>
#include <Resource/SwImage.h>
#include <Resource/SwPipeline.h>
#include <Scene/SwSystem.h>

#include <filesystem>
#include <string_view>

namespace SwWBOIT {

constexpr vk::ClearColorValue RVL_CLEAR_VALUE{1.f, 0.f, 0.f, 0.f};
constexpr vk::Format RVL_FORMAT{vk::Format::eR16Sfloat};
static constexpr std::string_view SHADERS_PATH{SHADERS_DIR "/WBOIT"};
static const std::filesystem::path VERTEX_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Composite.vert.spv"};
static const std::filesystem::path FRAGMENT_SHADER_PATH{std::filesystem::path(SHADERS_PATH) / "Composite.frag.spv"};

struct Resources {
    SwColorImage2D mAccumImage;
    SwColorImage2D mRvlImage;

    SwGraphicsPipelineBundle mCompositePipelineBundle;
    SwPipelineLayout mCompositePipelineLayout;

    SwDescriptorSet mCompositeDescriptorSet;
    SwDescriptorLayout mCompositeDescriptorLayout;
};
class System : public SwSystem, public SwSystem::Resizable {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;
    void refreshDependencies() override;

    void reInitializeOnResize() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
};
}  // namespace SwWBOIT