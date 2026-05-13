#include <Renderer/SwRenderer.h>
#include <Resources/SwResourceStager.h>

SwFactoryContext SwResourceStager::sRendererContext{};
std::unordered_map<SwResourceStager::DefaultImageOption, SwColorImage2D> SwResourceStager::sDefaultImages{};
SwStagingBuffer SwResourceStager::sImageStagingBuffer{};
SwStagingBuffer SwResourceStager::sMeshStagingBuffer{};
SwStagingBuffer SwResourceStager::sMaterialConstantsStagingBuffer{};
SwStagingBuffer SwResourceStager::sNodeTransformsStagingBuffer{};
SwStagingBuffer SwResourceStager::sBoundsStagingBuffer{};

void SwResourceStager::init(SwFactoryContext rendererContext) {
    sRendererContext = rendererContext;

    sImageStagingBuffer = SwBufferFactory::createStagingBuffer(IMAGE_STAGING_BUFFER_SIZE);
    sMeshStagingBuffer = SwBufferFactory::createStagingBuffer(MESH_STAGING_BUFFER_SIZE);
    sMaterialConstantsStagingBuffer = SwBufferFactory::createStagingBuffer(MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE);
    sNodeTransformsStagingBuffer = SwBufferFactory::createStagingBuffer(NODE_TRANSFORMS_STAGING_BUFFER_SIZE);
    sBoundsStagingBuffer = SwBufferFactory::createStagingBuffer(BOUNDS_STAGING_BUFFER_SIZE);

    constexpr std::uint32_t white = std::byteswap(0xFFFFFFFF);
    sDefaultImages.try_emplace(
        DefaultImageOption::White,
        SwImageFactory::createColorImage2D(&white, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t grey = std::byteswap(0xAAAAAAFF);
    sDefaultImages.try_emplace(
        DefaultImageOption::Grey,
        SwImageFactory::createColorImage2D(&grey, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t black = std::byteswap(0x000000FF);
    sDefaultImages.try_emplace(
        DefaultImageOption::Black,
        SwImageFactory::createColorImage2D(&black, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled, false)
    );
    constexpr std::uint32_t blue = std::byteswap(0x769DDBFF);
    sDefaultImages.try_emplace(
        DefaultImageOption::Blue,
        SwImageFactory::createColorImage2D(&blue, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled, false)
    );
    std::array<std::uint32_t, 16 * 16> pixels;
    for (std::uint32_t x = 0; x < 16; x++) {
        for (std::uint32_t y = 0; y < 16; y++) {
            constexpr std::uint32_t magenta = std::byteswap(0xFF00FFFF);
            pixels[static_cast<std::array<std::uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    sDefaultImages.try_emplace(
        DefaultImageOption::Checkerboard,
        SwImageFactory::createColorImage2D(pixels.data(), vk::Extent3D{16, 16, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled, false)
    );
}

void SwResourceStager::cleanup() { 
    sBoundsStagingBuffer.destroy(); 
    sNodeTransformsStagingBuffer.destroy();
    sMaterialConstantsStagingBuffer.destroy();
    sMeshStagingBuffer.destroy();
    sImageStagingBuffer.destroy();

    sDefaultImages.clear();
}
