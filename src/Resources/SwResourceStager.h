#pragma once

#include <Resources/SwImage.h>
#include <Resources/SwBuffer.h>

struct SwRendererContext;

class SwResourceStager {
public:
    enum class DefaultImageOption { White, Grey, Black, Blue, Checkerboard };

private:
    static SwRendererContext sRendererContext;

    static const std::uint32_t IMAGE_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;               // 256 MB
    static const std::uint32_t MESH_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;                // 256 MB
    static const std::uint32_t MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;  // 256 MB
    static const std::uint32_t NODE_TRANSFORMS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;     // 256 MB
    static const std::uint32_t BOUNDS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;              // 256 MB

public:
    static void init(SwRendererContext rendererContext);
    static void cleanup();

    static std::unordered_map<DefaultImageOption, SwColorImage2D> sDefaultImages;
    static SwStagingBuffer sImageStagingBuffer;
    static SwStagingBuffer sMeshStagingBuffer;
    static SwStagingBuffer sMaterialConstantsStagingBuffer;
    static SwStagingBuffer sNodeTransformsStagingBuffer;
    static SwStagingBuffer sBoundsStagingBuffer;
};
