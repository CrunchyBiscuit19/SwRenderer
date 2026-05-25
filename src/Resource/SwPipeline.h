#pragma once

#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct SwRendererContext;

class SwPipelineLayout {
private:
    vk::raii::PipelineLayout mLayout;

public:
    SwPipelineLayout();

    SwPipelineLayout(vk::raii::PipelineLayout layout);

    SwPipelineLayout(SwPipelineLayout&&) noexcept = default;
    SwPipelineLayout& operator=(SwPipelineLayout&&) noexcept = default;

    SwPipelineLayout(const SwPipelineLayout&) = delete;
    SwPipelineLayout& operator=(const SwPipelineLayout&) = delete;

    inline vk::PipelineLayout getRawLayout() { return *mLayout; };

    void destroy();
};

class SwPipelineBundle {
protected:
    static std::uint32_t sLatestPipelineID;
    std::uint32_t mId;
    vk::raii::Pipeline mPipeline;
    vk::PipelineLayout mLayout;

public:
    SwPipelineBundle();

    SwPipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout);

    SwPipelineBundle(SwPipelineBundle&&) noexcept = default;
    SwPipelineBundle& operator=(SwPipelineBundle&&) noexcept = default;

    SwPipelineBundle(const SwPipelineBundle&) = delete;
    SwPipelineBundle& operator=(const SwPipelineBundle&) = delete;

    inline std::uint32_t getID() { return mId; };

    inline vk::Pipeline getRawPipeline() { return *mPipeline; };

    inline vk::PipelineLayout getRawLayout() { return mLayout; };
};

class SwGraphicsPipelineBundle : public SwPipelineBundle {
public:
    SwGraphicsPipelineBundle() = default;

    SwGraphicsPipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout);
};

class SwComputePipelineBundle : public SwPipelineBundle {
public:
    SwComputePipelineBundle() = default;

    SwComputePipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout); 
};

class SwPipelineFactory {
protected:
    static SwRendererContext sRendererContext;

    static constexpr std::string DEFAULT_SHADER_ENTRY_POINT{"main"};
    static constexpr std::uint32_t MIN_NUM_SHADER_STAGES{2};

public:
    static void init(SwRendererContext rendererContext);

    static SwPipelineLayout createPipelineLayout(vk::ArrayProxy<vk::DescriptorSetLayout> layouts, vk::ArrayProxy<vk::PushConstantRange> pushConstantRanges);

    static vk::PushConstantRange createPushConstantRange(vk::ShaderStageFlags stageFlags, std::uint32_t offset, std::uint32_t size);
};

class SwGraphicsPipelineFactory : public SwPipelineFactory {
public:
    struct SwGraphicsPipelineOptions {
        vk::ShaderModule mVertexShader;
        vk::ShaderModule mFragmentShader;
        vk::PipelineLayout mLayout;
        vk::PrimitiveTopology mTopology;
        vk::PolygonMode mPolygonMode;
        vk::CullModeFlags mCullMode;
        vk::FrontFace mFrontFace;
        bool mMultisamplingEnabled;
        bool mSampleShadingEnabled;
        std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>> mColorAttachments;
        vk::Format mDepthFormat;
        bool mDepthTestEnabled;
        bool mDepthWriteEnabled;
        vk::CompareOp mDepthCompareOp;
    };

    static SwGraphicsPipelineBundle createGraphicsPipeline(SwGraphicsPipelineOptions options);

private:
    static void setShaders(
        std::vector<vk::PipelineShaderStageCreateInfo>& pipelineShaderStageCreateInfos, std::uint32_t& numStages, vk::ShaderModule vertexShader,
        vk::ShaderModule fragmentShader
    );
    static void setInputTopology(vk::PipelineInputAssemblyStateCreateInfo& pipelineInputAssemblyStateCreateInfo, vk::PrimitiveTopology topology);
    static void setPolygonMode(vk::PipelineRasterizationStateCreateInfo& pipelineRasterizationStateCreateInfo, vk::PolygonMode mode);
    static void setCullMode(
        vk::PipelineRasterizationStateCreateInfo& pipelineRasterizationStateCreateInfo, vk::CullModeFlags cullMode, vk::FrontFace frontFace
    );
    static void disableMultisampling(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo);
    static void enableMultisampling(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo);
    static void disableSampleShading(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo);
    static void enableSampleShading(vk::PipelineMultisampleStateCreateInfo& pipelineMultiSampleStateCreateInfo);
    static void setColorAttachments(
        vk::PipelineRenderingCreateInfo& pipelineRenderingCreateInfo, vk::PipelineColorBlendStateCreateInfo& pipelineColorBlendStateCreateInfo,
        std::span<vk::Format> colorAttachmentFormats, std::span<vk::PipelineColorBlendAttachmentState> colorBlendAttachmentStates
    );
    static void setDepthFormat(vk::PipelineRenderingCreateInfo& pipelineRenderingCreateInfo, vk::Format format);
    static void disableDepthTest(vk::PipelineDepthStencilStateCreateInfo& pipelineDepthStencilStateCreateInfo);
    static void enableDepthTest(vk::PipelineDepthStencilStateCreateInfo& pipelineDepthStencilStateCreateInfo, bool depthWriteEnable, vk::CompareOp op);
};

class SwComputePipelineFactory : public SwPipelineFactory {
public:
    struct SwComputePipelineOptions {
        vk::ShaderModule mComputeShader;
        vk::PipelineLayout mLayout;
    };

    static SwComputePipelineBundle createComputePipeline(SwComputePipelineOptions options);

private:
    static void setShaders(
        vk::PipelineShaderStageCreateInfo& pipelineShaderStageCreateInfos, vk::ShaderModule computeShader
    );
};
