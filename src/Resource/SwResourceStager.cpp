#include <Renderer/SwRenderer.h>
#include <Resource/SwResourceStager.h>

SwFactoryContext SwResourceStager::sRendererContext{};
SwStagingBuffer SwResourceStager::sMeshStagingBuffer;
SwStagingBuffer SwResourceStager::sMaterialConstantsStagingBuffer;
SwStagingBuffer SwResourceStager::sNodeTransformsStagingBuffer;
SwStagingBuffer SwResourceStager::sBoundsStagingBuffer;

void SwResourceStager::init(SwFactoryContext rendererContext) {
    sRendererContext = rendererContext;

    sMeshStagingBuffer = SwBufferFactory::createStagingBuffer(MESH_STAGING_BUFFER_SIZE);
    sMaterialConstantsStagingBuffer = SwBufferFactory::createStagingBuffer(MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE);
    sNodeTransformsStagingBuffer = SwBufferFactory::createStagingBuffer(NODE_TRANSFORMS_STAGING_BUFFER_SIZE);
    sBoundsStagingBuffer = SwBufferFactory::createStagingBuffer(BOUNDS_STAGING_BUFFER_SIZE);
}

void SwResourceStager::cleanup() { 
    sBoundsStagingBuffer.destroy(); 
    sNodeTransformsStagingBuffer.destroy();
    sMaterialConstantsStagingBuffer.destroy();
    sMeshStagingBuffer.destroy();
}
