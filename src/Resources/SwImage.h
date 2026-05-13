#pragma once

#include <Resources/SwSemaphore.h>
#include <vk_mem_alloc.h>

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwFactoryContext;

class SwImage {
protected:
    std::vector<vk::Format> mFormats;
    vk::Extent3D mExtent;
    vk::ImageLayout mCurrentLayout;
    vk::PipelineStageFlagBits2 mCurrentStage;
    vk::AccessFlags2 mCurrentAccess;

    SwImage();

    SwImage(std::vector<vk::Format> formats, vk::Extent3D extent);

public:
    virtual void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) = 0;

    virtual void transition(vk::CommandBuffer cmd, vk::ImageLayout nextLayout, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) = 0;

    inline vk::Extent3D getExtent() { return mExtent; }
    inline vk::Format getFormat(size_t i = 0) { return mFormats[i]; }

    inline void setCurrentLayout(vk::ImageLayout layout) { mCurrentLayout = layout; }
    inline void setCurrentStage(vk::PipelineStageFlagBits2 stage) { mCurrentStage = stage; }
    inline void setCurrentAccess(vk::AccessFlags2 access) { mCurrentAccess = access; }

    SwImage(SwImage&&) noexcept = default;
    SwImage& operator=(SwImage&&) noexcept = default;

    SwImage(const SwImage&) = delete;
    SwImage& operator=(const SwImage&) = delete;

    virtual ~SwImage() = default;
};

class SwSwapchainImage : public SwImage {
private:
    vk::Image mImage;
    std::vector<vk::raii::ImageView> mImageViews;
    SwSemaphore mRenderedSemaphore;

public:
    SwSwapchainImage(
        vk::Image image, std::vector<vk::raii::ImageView> imageViews, SwSemaphore renderedSemaphore, std::vector<vk::Format> formats, vk::Extent3D extent
    );

    void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) override;

    void transition(vk::CommandBuffer cmd, vk::ImageLayout nextLayout, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) override;

    inline vk::Image getRawImage() const { return mImage; }

    SwSwapchainImage(SwSwapchainImage&&) noexcept = default;
    SwSwapchainImage& operator=(SwSwapchainImage&&) noexcept = default;

    SwSwapchainImage(const SwSwapchainImage&) = delete;
    SwSwapchainImage& operator=(const SwSwapchainImage&) = delete;
};

class SwAllocatedImage : public SwImage {
protected:
    vk::raii::Image mImage;
    std::vector<vk::raii::ImageView> mImageViews;
    vk::ClearValue mClearValue;
    vk::ImageAspectFlags mAspect;
    VmaAllocator mAllocator;
    VmaAllocation mAllocation;
    bool mMipmapped;
    std::uint32_t mMipLevels;

    SwAllocatedImage();

    SwAllocatedImage(
        vk::raii::Image image, std::vector<vk::raii::ImageView> imageViews, std::vector<vk::Format> formats, vk::Extent3D extent, bool mipmapped,
        vk::ClearValue clearValue, vk::ImageAspectFlags aspect, const VmaAllocator mAllocator, VmaAllocation mAllocation
    );

    void generateMipmaps(vk::CommandBuffer cmd, std::uint32_t numFaces);

public:
    void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) override;

    void transition(vk::CommandBuffer cmd, vk::ImageLayout nextLayout, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) override;

    void copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect);
    void copyFrom(vk::CommandBuffer cmd, SwSwapchainImage& source);
    void copyFrom(vk::CommandBuffer cmd, SwAllocatedImage& source);

    virtual void generateMipmaps(vk::CommandBuffer cmd) = 0;

    inline vk::Image getRawImage() { return *mImage; }

    inline bool isMipmapped() const { return mMipmapped; }

    void destroy();

    SwAllocatedImage(SwAllocatedImage&&) noexcept;
    SwAllocatedImage& operator=(SwAllocatedImage&&) noexcept;

    SwAllocatedImage(const SwAllocatedImage&) = delete;
    SwAllocatedImage& operator=(const SwAllocatedImage&) = delete;

    ~SwAllocatedImage();
};

class SwColorImage2D : public SwAllocatedImage {
public:
    SwColorImage2D();

    SwColorImage2D(
        vk::raii::Image image, std::vector<vk::raii::ImageView> imageViews, std::vector<vk::Format> formats, vk::Extent3D extent, bool mipmapped,
        vk::ClearValue clearValue, const VmaAllocator mAllocator, VmaAllocation mAllocation
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    SwColorImage2D(SwColorImage2D&&) noexcept = default;
    SwColorImage2D& operator=(SwColorImage2D&&) noexcept = default;

    SwColorImage2D(const SwColorImage2D&) = delete;
    SwColorImage2D& operator=(const SwColorImage2D&) = delete;
};

class SwDepthImage2D : public SwAllocatedImage {
public:
    SwDepthImage2D();

    SwDepthImage2D(
        vk::raii::Image image, std::vector<vk::raii::ImageView> imageViews, std::vector<vk::Format> formats, vk::Extent3D extent, bool mipmapped,
        vk::ClearValue clearValue, const VmaAllocator mAllocator, VmaAllocation mAllocation
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    SwDepthImage2D(SwDepthImage2D&&) noexcept = default;
    SwDepthImage2D& operator=(SwDepthImage2D&&) noexcept = default;

    SwDepthImage2D(const SwDepthImage2D&) = delete;
    SwDepthImage2D& operator=(const SwDepthImage2D&) = delete;
};

class SwColorImageCubemap : public SwAllocatedImage {
public:
    SwColorImageCubemap();

    SwColorImageCubemap(
        vk::raii::Image image, std::vector<vk::raii::ImageView> imageViews, std::vector<vk::Format> formats, vk::Extent3D extent, bool mipmapped,
        vk::ClearValue clearValue, const VmaAllocator mAllocator, VmaAllocation mAllocation
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    SwColorImageCubemap(SwColorImageCubemap&&) noexcept = default;
    SwColorImageCubemap& operator=(SwColorImageCubemap&&) noexcept = default;

    SwColorImageCubemap(const SwColorImageCubemap&) = delete;
    SwColorImageCubemap& operator=(const SwColorImageCubemap&) = delete;
};

class SwImageFactory {
private:
    enum class SwImageType { SwColorImage2D, SwDepthImage2D, SwColorImageCubemap };
    struct SwImageConstructionInfo {
        vk::raii::Image mImage;
        std::vector<vk::raii::ImageView> mImageViews;
        std::vector<vk::Format> mFormats;
        vk::Extent3D mExtent;
        bool mMipmapped;
        vk::ClearValue mClearValue;
        const VmaAllocator mAllocator;
        VmaAllocation mAllocation;
    };

    static SwFactoryContext sRendererContext;

    static SwImageConstructionInfo prepareImageConstructionInfo(
        SwImageType swImageType, const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue
    );

    static void fillImageData(SwImageType swImageType, const void* data, SwAllocatedImage& image);

public:
    static const uint32_t NUM_CUBEMAP_FACES{6};

    static void init(SwFactoryContext rendererContext);

    static SwColorImage2D createColorImage2D(
        const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue = vk::ClearValue()
    );

    static SwDepthImage2D createDepthImage2D(
        const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false,
        vk::ClearValue clearValue = vk::ClearValue()
    );

    static SwColorImageCubemap createColorImageCubemap(
        const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, vk::ClearValue clearValue = vk::ClearValue()
    );
};