#pragma once

#include <Resource/SwImage.h>
#include <Resource/SwBuffer.h>

struct SwFactoryContext;

class SwResourceStager {
private:
    static SwFactoryContext sRendererContext;

    static const std::uint32_t MESH_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;                // 256 MB
    static const std::uint32_t MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;  // 256 MB
    static const std::uint32_t NODE_TRANSFORMS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;     // 256 MB
    static const std::uint32_t BOUNDS_STAGING_BUFFER_SIZE = 256 * 1024 * 1024;              // 256 MB

public:
    static void init(SwFactoryContext rendererContext);
    static void cleanup();

    static SwStagingBuffer sMeshStagingBuffer;
    static SwStagingBuffer sMaterialConstantsStagingBuffer;
    static SwStagingBuffer sNodeTransformsStagingBuffer;
    static SwStagingBuffer sBoundsStagingBuffer;
};
