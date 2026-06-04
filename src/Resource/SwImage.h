
#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwSampler.h>
#include <Resource/SwSemaphore.h>
#include <Scene/SwDependency.h>
#include <vk_mem_alloc.h>

#include <deque>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwImage {
protected:
    vk::Format mMainFormat;
    std::vector<vk::Format> mOtherFormats;
    vk::Extent3D mExtent;
    vk::ImageAspectFlags mAspect;
    vk::ImageLayout mCurrentLayout;
    vk::PipelineStageFlags2 mCurrentStage;
    vk::AccessFlags2 mCurrentAccess;

    SwImage();

    SwImage(vk::Format mainFormat, vk::Extent3D extent, vk::ImageAspectFlags aspect, std::vector<vk::Format> otherFormats = {});

public:
    virtual void emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) = 0;

    virtual void emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout) = 0;
    void emitTransition(vk::CommandBuffer cmd, SwDependency::ImageDepType imageDepType);

    virtual void copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect) = 0;
    void copyFrom(vk::CommandBuffer cmd, SwImage& source);

    virtual vk::RenderingAttachmentInfo generateRenderingAttachment(
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore
    );
    virtual vk::RenderingAttachmentInfo generateRenderingAttachment(
        std::uint32_t otherImageViewIndex, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eLoad,
        vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore
    );

    inline vk::Extent3D getExtent() { return mExtent; }
    inline vk::Format getMainFormat() { return mMainFormat; }
    inline vk::ImageAspectFlags getAspect() const { return mAspect; }

    inline vk::ImageLayout getCurrentLayout() { return mCurrentLayout; }
    inline void setCurrentLayout(vk::ImageLayout layout) { mCurrentLayout = layout; }
    inline vk::PipelineStageFlags2 getCurrentStage() { return mCurrentStage; }
    inline void setCurrentStage(vk::PipelineStageFlags2 stage) { mCurrentStage = stage; }
    inline vk::AccessFlags2 getCurrentAccess() { return mCurrentAccess; }
    inline void setCurrentAccess(vk::AccessFlags2 access) { mCurrentAccess = access; }

    virtual vk::Image getRawImage() = 0;
    virtual vk::ImageView getRawMainImageView() = 0;
    virtual vk::ImageView getRawOtherImageView(std::uint32_t i) = 0;
    virtual vk::ClearValue getClearValue() = 0;

    SwImage(SwImage&&) noexcept = default;
    SwImage& operator=(SwImage&&) noexcept = default;

    SwImage(const SwImage&) = delete;
    SwImage& operator=(const SwImage&) = delete;

    virtual ~SwImage() = default;
};

class SwSwapchainImage : public SwImage {
private:
    vk::Image mImage;
    vk::raii::ImageView mMainImageView;
    std::deque<vk::raii::ImageView> mOtherImageViews;
    SwSemaphore mRenderedSemaphore;

public:
    SwSwapchainImage();
    SwSwapchainImage(
        vk::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, SwSemaphore renderedSemaphore,
        std::vector<vk::Format> otherFormats = {}, std::deque<vk::raii::ImageView> otherImageViews = {}
    );

    inline vk::Image getRawImage() override { return mImage; }
    inline vk::ImageView getRawMainImageView() override { return *mMainImageView; }
    inline vk::ImageView getRawOtherImageView(std::uint32_t i) override { return *mOtherImageViews[i]; }
    inline vk::ClearValue getClearValue() override { return vk::ClearColorValue(); };
    inline SwSemaphore& getRenderedSemaphore() { return mRenderedSemaphore; }

    void emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) override;
    using SwImage::emitTransition;
    void emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout) override;

    using SwImage::copyFrom;
    void copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect) override;

    SwSwapchainImage(SwSwapchainImage&&) noexcept = default;
    SwSwapchainImage& operator=(SwSwapchainImage&&) noexcept = default;

    SwSwapchainImage(const SwSwapchainImage&) = delete;
    SwSwapchainImage& operator=(const SwSwapchainImage&) = delete;
};

class SwAllocatedImage : public SwImage {
protected:
    vk::raii::Image mImage;
    vk::raii::ImageView mMainImageView;
    std::deque<vk::raii::ImageView> mOtherImageViews;
    vk::ImageUsageFlags mUsage;
    vk::ClearValue mClearValue;
    VmaAllocator mAllocator{nullptr};
    VmaAllocation mAllocation{nullptr};
    bool mMipmapped;
    std::uint32_t mMipLevels;

    SwAllocatedImage();

    SwAllocatedImage(
        vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage,
        vk::ClearValue clearValue, vk::ImageAspectFlags aspect, const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped,
        std::vector<vk::Format> otherFormats = {}, std::deque<vk::raii::ImageView> otherImageViews = {}
    );

    void generateMipmaps(vk::CommandBuffer cmd, std::uint32_t numFaces);

public:
    void emitBarrier(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess) override;
    using SwImage::emitTransition;
    void emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout) override;
    void emitTransition(vk::CommandBuffer cmd, vk::PipelineStageFlags2 nextStage, vk::AccessFlags2 nextAccess, vk::ImageLayout nextLayout, std::uint32_t mipLevel);

    using SwImage::copyFrom;
    void copyFrom(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D srcSize, vk::ImageAspectFlags srcAspect) override;

    virtual void generateMipmaps(vk::CommandBuffer cmd) = 0;

    inline vk::Image getRawImage() override { return *mImage; }
    inline vk::ImageView getRawMainImageView() override { return *mMainImageView; }
    inline vk::ImageView getRawOtherImageView(std::uint32_t i) override { return *mOtherImageViews[i]; }
    inline vk::ClearValue getClearValue() override { return mClearValue; };
    inline bool isMipmapped() const { return mMipmapped; }
    inline bool isReady() const { return mAllocation != nullptr; }

    void addImageView(
        vk::Format format, vk::ImageAspectFlags aspect, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t baseMipLevel = 0,
        std::uint32_t levelCount = 1, std::uint32_t baseArrayLayer = 0, std::uint32_t layerCount = 1
    );

    virtual void resize(vk::Extent3D newExtent) = 0;

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
        vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage,
        vk::ClearValue clearValue, const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats = {},
        std::deque<vk::raii::ImageView> otherImageViews = {}
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    void resize(vk::Extent3D newExtent) override;

    SwColorImage2D(SwColorImage2D&&) noexcept = default;
    SwColorImage2D& operator=(SwColorImage2D&&) noexcept = default;

    SwColorImage2D(const SwColorImage2D&) = delete;
    SwColorImage2D& operator=(const SwColorImage2D&) = delete;
};

class SwDepthImage2D : public SwAllocatedImage {
public:
    SwDepthImage2D();

    SwDepthImage2D(
        vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage,
        vk::ClearValue clearValue, const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats = {},
        std::deque<vk::raii::ImageView> otherImageViews = {}
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    void resize(vk::Extent3D newExtent) override;

    SwDepthImage2D(SwDepthImage2D&&) noexcept = default;
    SwDepthImage2D& operator=(SwDepthImage2D&&) noexcept = default;

    SwDepthImage2D(const SwDepthImage2D&) = delete;
    SwDepthImage2D& operator=(const SwDepthImage2D&) = delete;
};

class SwColorImageCubemap : public SwAllocatedImage {
public:
    SwColorImageCubemap();

    SwColorImageCubemap(
        vk::raii::Image image, vk::Format mainFormat, vk::Extent3D extent, vk::raii::ImageView mainImageView, vk::ImageUsageFlags usage,
        vk::ClearValue clearValue, const VmaAllocator allocator, VmaAllocation allocation, bool mipmapped, std::vector<vk::Format> otherFormats = {},
        std::deque<vk::raii::ImageView> otherImageViews = {}
    );

    void generateMipmaps(vk::CommandBuffer cmd) override;

    void resize(vk::Extent3D newExtent) override;

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
        vk::raii::ImageView mMainImageView;
        vk::Format mMainFormat;
        vk::Extent3D mExtent;
        bool mMipmapped;
        vk::ImageUsageFlags mUsage;
        vk::ClearValue mClearValue;
        const VmaAllocator mAllocator;
        VmaAllocation mAllocation;
    };

    static constexpr std::uint32_t IMAGE_STAGING_BUFFER_SIZE{256 * (1 << 20)};  // 256 MB
    static SwStagingBuffer sImageStagingBuffer;


    static std::uint32_t getFormatTexelSize(vk::Format format);

    static SwImageConstructionInfo prepareImageConstructionInfo(
        SwImageType swImageType, const void* data, vk::Format mainFormat, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped,
        vk::ClearValue clearValue
    );

    static void fillImageData(SwImageType swImageType, const void* data, SwAllocatedImage& image);

public:
    enum class SwDefaultImageOption { White, Grey, Black, Blue, Checkerboard };

    static std::unordered_map<SwDefaultImageOption, SwColorImage2D> sDefaultImages;

    static constexpr uint32_t NUM_CUBEMAP_FACES{6};

    static void init();

    static vk::raii::ImageView createImageView(
        vk::Image image, vk::Format format, vk::ImageAspectFlags aspect, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t baseMipLevel = 0,
        std::uint32_t levelCount = 1, std::uint32_t baseArrayLayer = 0, std::uint32_t layerCount = 1
    );

    static SwColorImage2D createColorImage2D(
        const void* data, vk::Format mainFormat, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped,
        vk::ClearValue clearValue = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f)
    );

    static SwDepthImage2D createDepthImage2D(
        const void* data, vk::Format mainFormat, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped = false,
        vk::ClearValue clearValue = vk::ClearValue()
    );

    static SwColorImageCubemap createColorImageCubemap(
        const void* data, vk::Format mainFormat, vk::Extent3D extent, vk::ImageUsageFlags usage, bool mipmapped,
        vk::ClearValue clearValue = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f)
    );

    static void cleanup();
};