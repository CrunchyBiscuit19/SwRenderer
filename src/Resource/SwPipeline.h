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

    inline vk::PipelineLayout getHandle() { return *mLayout; };

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

    inline virtual vk::PipelineBindPoint getBindPoint() = 0;

    inline std::uint32_t getID() { return mId; };

    inline vk::Pipeline getPipelineHandle() { return *mPipeline; };

    inline vk::PipelineLayout getLayouthandle() { return mLayout; };
};

class SwGraphicsPipelineBundle : public SwPipelineBundle {
public:
    SwGraphicsPipelineBundle() = default;

    SwGraphicsPipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout);

    inline vk::PipelineBindPoint getBindPoint() override { return vk::PipelineBindPoint::eGraphics; }
};

class SwComputePipelineBundle : public SwPipelineBundle {
public:
    SwComputePipelineBundle() = default;

    SwComputePipelineBundle(vk::raii::Pipeline pipeline, vk::PipelineLayout layout); 

    inline vk::PipelineBindPoint getBindPoint() override { return vk::PipelineBindPoint::eCompute; }
};

class SwPipelineFactory {
protected:
    static constexpr std::string DEFAULT_SHADER_ENTRY_POINT{"main"};

public:
    static void init();

    static SwPipelineLayout createPipelineLayout(
        std::string name, vk::ArrayProxy<vk::DescriptorSetLayout> layouts, vk::ArrayProxy<vk::PushConstantRange> pushConstantRanges
    );
};

class SwGraphicsPipelineFactory : public SwPipelineFactory {
public:
    struct SwGraphicsPipelineOptions {
        std::optional<vk::ShaderModule> mVertexShader{std::nullopt};
        std::optional<vk::ShaderModule> mFragmentShader{std::nullopt};
        std::string mVertexEntryPoint{DEFAULT_SHADER_ENTRY_POINT};
        std::string mFragmentEntryPoint{DEFAULT_SHADER_ENTRY_POINT};
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

    static SwGraphicsPipelineBundle createGraphicsPipeline(std::string name, SwGraphicsPipelineOptions options);

private:
    static void setShaders(
        std::vector<vk::PipelineShaderStageCreateInfo>& pipelineShaderStageCreateInfos, std::optional<vk::ShaderModule> vertexShader,
        std::optional<vk::ShaderModule> fragmentShader, const std::string& vertexEntryPoint = DEFAULT_SHADER_ENTRY_POINT,
        const std::string& fragmentEntryPoint = DEFAULT_SHADER_ENTRY_POINT
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
        std::string mComputeEntryPoint{DEFAULT_SHADER_ENTRY_POINT};
    };

    static SwComputePipelineBundle createComputePipeline(std::string name, SwComputePipelineOptions options);

private:
    static void setShaders(
        vk::PipelineShaderStageCreateInfo& pipelineShaderStageCreateInfos, vk::ShaderModule computeShader,
        const std::string& computeEntryPoint = DEFAULT_SHADER_ENTRY_POINT
    );
};
