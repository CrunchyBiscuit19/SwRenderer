#pragma once

#include <glm/glm.hpp>

struct Plane {
    glm::vec3 normal;
    float d;

    Plane() : normal(glm::vec3(0.f)), d(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : normal(glm::normalize(n)), d(glm::dot(glm::normalize(n), p)) {}
};




//struct CullerCompactPushConstants {
//    vk::DeviceAddress mPreCullRenderItemsBuffer;
//    vk::DeviceAddress postCullRenderItemsBuffer;
//    vk::DeviceAddress postCullRenderItemsCountBuffer;
//    std::uint32_t preCullRenderItemsLimit;
//};
//
//struct CullerDepthPyramidPushConstants {
//    glm::uvec2 depthPyramidExtent;
//    glm::uvec2 depthFullExtent;
//    glm::vec2 depthFullRatio;
//    std::uint32_t level;
//    bool readFromFull;
//};
//
//class Culler {
//    Renderer* mRenderer;
//
//public:
//    PipelineBundle mCompactPipelineBundle;
//    vk::raii::PipelineLayout mCompactPipelineLayout;
//    CullerCompactPushConstants mCompactPushConstants;
//
//    PipelineBundle mDepthPyramidPipelineBundle;
//    vk::raii::PipelineLayout mDepthPyramidPipelineLayout;
//    vk::raii::DescriptorSet mDepthPyramidDescriptorSet;
//    vk::raii::DescriptorSetLayout mDepthPyramidDescriptorSetLayout;
//    AllocatedImage mDepthPyramidImage;
//    std::vector<vk::raii::ImageView> mDepthPyramidMipViews;
//    std::uint32_t mDepthPyramidLevels{0};
//    vk::Extent3D mDepthPyramidExtent;
//    CullerDepthPyramidPushConstants mDepthPyramidPushConstants;
//
//    bool mFreezeCulling{false};
//
//    Culler(Renderer* renderer);
//
//    void init();
//    void initDepthPyramidImage();
//    void initDepthPyramidDescriptor();
//    void writeDepthPyramidDescriptor();
//    void initDepthPyramidPipeline();
//    void initDepthPyramidPushConstants();
//    void initResetPipeline();
//    void initCullDescriptor();
//    void writeCullDescriptor();
//    void initCullPipeline();
//    void initCullPushConstants();
//    void initCompactPipeline();
//
//    void resizeCuller();
//
//    void cleanup();
//};
