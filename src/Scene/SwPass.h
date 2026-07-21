#pragma once

#include <Resource/SwBuffer.h>
#include <Resource/SwImage.h>
#include <Scene/SwDependency.h>

#include <functional>
#include <string>
#include <vector>

class SwPass {
public:
    enum class Type {
        ClearImages,
        IBLSkybox,
        CullEarlyReset,
        CullEarlyTest,
        CullEarlyCompact,
        GeometryEarlyOpaque,
        CullPrepOcclusion,
        CullLateReset,
        CullLateTest,
        CullLateCompact,
        CullPublishCount,
        GeometryZPass,
        LightingReset,
        LightingClustersBuild,
        LightingClustersMarkActive,
        LightingClustersCompactActive,
        LightingLightsCull,
        LightingClustersLightSelect,
        LightingShadowsCull,
        LightingShadowsDraw,
        GeometryLateOpaque,
        GeometryMasked,
        GeometryTransparent,
        PickDraw,
        PickReadback,
        PickSelect,
        WBOITComposite,
        Tonemap,
        FXAA,
        CopyToSwapchain,
        Gui
    };

private:
    Type mPassType{Type::ClearImages};
    std::function<void(vk::CommandBuffer)> mCallback;
    bool mMustRun{false};
    bool mPruned{false};

    SwDependency mDeps;

public:
    SwPass() = default;

    SwPass(Type passType, std::function<void(vk::CommandBuffer)> callback, bool mustRun = false);

    Type getPassType() const { return mPassType; }
    bool isPruned() const { return mPruned; }
    bool isMustRun() const { return mMustRun; }
    void setPruned(bool pruned) { mPruned = pruned; }
    SwDependency& getDeps() { return mDeps; }

    void execute(vk::CommandBuffer cmd);

    static vk::RenderingInfo generateRenderingInfo(
        vk::Extent2D renderExtent, vk::ArrayProxy<vk::RenderingAttachmentInfo> colorAttachments, vk::ArrayProxy<vk::RenderingAttachmentInfo> depthAttachment
    );
    static void setViewportScissors(vk::CommandBuffer cmd, vk::Extent3D imageExtent);
};
