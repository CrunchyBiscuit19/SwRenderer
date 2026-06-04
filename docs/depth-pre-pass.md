# Depth Pre-Pass Implementation Plan

## Overview

A depth pre-pass renders opaque (and alpha-masked) geometry z-only before the main colour
pass. This enables **same-frame** HiZ occlusion culling — the depth pyramid is built from
*this* frame's depth instead of last frame's, so there is no temporal lag — and it lets the
main colour pass skip shading hidden fragments.

The frame is restructured into two culling phases, each in its own compute shader:

1. **Frustum phase** (`SwCullWorkFrustum`) — frustum-cull every candidate render instance,
   compact the survivors into a draw list, draw them z-only into the depth buffer, then build
   the depth pyramid from that depth.
2. **Occlusion phase** (`SwCullWorkOcclusion`) — occlusion-cull **only the frustum survivors**
   against the freshly built pyramid, compact, and draw the survivors in the colour pass.

The phases write into **separate per-phase buffers**, so there is no mid-frame reset and no
buffer aliasing between phases (see §3).

---

## 0  Key invariants (read before implementing)

These are the non-obvious constraints that make the rest of the plan correct.

1. **Reverse-Z.** The camera projection is reversed-Z (`SwCamera.h`: "Y-flipped, reversed-Z";
   every existing pipeline uses `vk::CompareOp::eGreaterOrEqual`). The depth pre-pass must
   therefore use **`eGreaterOrEqual`**, *not* `eLessOrEqual`. Depth is cleared to `0.0` (far)
   by `ClearImages`.

2. **`eEqual` requires set equality.** The main opaque pass switches to `eEqual` with depth
   writes **off**. This is only correct if **every primitive drawn in the main pass was also
   drawn in the pre-pass**, with the identical vertex transform (same vertex shader → bitwise
   identical interpolated depth). Consequences:
   - The occlusion (phase-2) draw set is a strict subset of the frustum (phase-1) pre-pass
     set, so this holds for opaque geometry automatically.
   - **Alpha-masked geometry must be in the pre-pass too** (with an alpha-test fragment
     shader). Anything drawn with `eEqual` in the main pass but absent from the pre-pass will
     fail the equality test and vanish. See §6.
   - Transparent geometry is never in the pre-pass; the transparent pass keeps its current
     depth state. The `eEqual` change applies to `GeometryOpaque` only.

3. **Occlusion must only see frustum survivors.** Running occlusion over the *full* instance
   list is wrong: `occlusionCull` returns `true` for anything behind the near plane
   (`clipPos.w <= 0`, the conservative fallback at `SwCullWork.comp.slang:57`), so
   frustum-rejected / behind-camera instances would leak into the colour set. **Barriers do
   not fix this** — a barrier orders memory, it does not restrict which elements a dispatch
   visits. The fix is to feed phase 2 only the survivors, via a compacted survivor list +
   indirect dispatch (§4, the documented approach) or a per-instance flag (§4, alternative).
   With either, the phase-2 shader runs occlusion **only** — no frustum re-test.

4. **The two phases own separate state.** Each phase has its own per-item instance-count
   scratch, its own visible-index buffer, and its own compacted draw list. All are zeroed once
   by `CullReset` at the top of the frame; nothing is reset mid-frame and no buffer is shared
   across phases. The only cross-phase data flow is phase 1 → phase 2's survivor list (§4).

5. **Indirect dispatch rounds up.** The phase-2 dispatch covers whole workgroups, so the
   occlusion shader must still bounds-check `if (i >= survivorCount) return;` before touching
   the survivor list (§4).

---

## 1  Pass Types (`SwPass.h`)

```cpp
enum class Type {
    ClearImages,
    CullReset,
    CullWorkFrustum,
    CullCompactFrustum,
    GeometryDepthPrePass,
    CullDepthPyramid,        // build pyramid from this-frame depth
    CullPrepOcclusion,       // write the phase-2 indirect dispatch command (§4)
    CullWorkOcclusion,
    CullCompactOcclusion,
    PickDraw,
    PickReadback,
    PickWork,
    SkyboxWork,
    GeometryOpaque,
    GeometryTransparent,
    WBOITComposite,
    CopyToSwapchain,
    Gui
};
```

`CullPrepOcclusion` and `CullDepthPyramid` are independent; both must precede
`CullWorkOcclusion`. There is no `CullResetOcclusion` — per §0.4 the single frame-start
`CullReset` zeroes both phases' buffers.

---

## 2  Two work shaders (`SwCull`)

Replace the single `SwCullWork.comp` with two shaders that share the cull math helpers
(`frustumCull` / `occlusionCull` move into the `SwCull` module / a shared `.slang`):

### 2a  `SwCullWorkFrustum.comp.slang`

Dispatched directly over the full instance list (CPU knows the size). For each instance:

```slang
[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint i = in.mDispatchThreadID.x;
    if (i >= frustumPc.mRenderInstancesLimit) return;

    SwRenderInstance ri = frustumPc.mRenderInstancesBuffer[i];
    // ... gather item / instance / bounds / nodeTransform / frustum ...
    if (!frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform)) return;

    // (a) record this instance as a frustum survivor for phase 2 (§4)
    uint slot; InterlockedAdd(*frustumPc.mSurvivorCountBuffer, 1, slot);
    frustumPc.mSurvivorInstancesBuffer[slot] = ri;

    // (b) produce the phase-1 drawable set for the depth pre-pass
    uint local; InterlockedAdd(frustumPc.mPhase1ItemCounts[ri.mRenderItemsIndex], 1, local);
    uint visIdx = item->mFirstRenderInstance + local;
    frustumPc.mPhase1VisibleIndexBuffer[visIdx] = ri.mSceneInstanceIndex;
}
```

So phase 1 does two things: it appends survivors to a compact list (for phase 2), and it fills
the phase-1 per-item counts + visible-index so the depth pre-pass can draw the frustum set.

### 2b  `SwCullWorkOcclusion.comp.slang`

Dispatched **indirectly** over the survivor list (§4). Occlusion only — no frustum re-test:

```slang
[numthreads(SwCull::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint i = in.mDispatchThreadID.x;
    if (i >= occlusionPc.mSurvivorCountBuffer[0]) return;   // §0.5 round-up guard

    SwRenderInstance ri = occlusionPc.mSurvivorInstancesBuffer[i];
    // ... gather item / bounds / nodeTransform / perspective ...
    // corners already need re-projecting; reuse occlusionCull as today
    if (!occlusionCull(aabbCorners, perspective, depthPyramidImage, occlusionPc.mDrawExtents)) return;

    uint _; InterlockedAdd(*occlusionPc.mRenderInstancesCountBuffer, 1, _);   // stats
    uint local; InterlockedAdd(occlusionPc.mPhase2ItemCounts[ri.mRenderItemsIndex], 1, local);
    uint visIdx = item->mFirstRenderInstance + local;
    occlusionPc.mPhase2VisibleIndexBuffer[visIdx] = ri.mSceneInstanceIndex;
}
```

Both shaders use the **master** `mRenderItemsBuffer` only as a read-only lookup (for
`mFirstRenderInstance`, bounds index, node-transform index); `ri.mRenderItemsIndex` indexes the
original, uncompacted item array, so the lookup table must never be a compacted buffer.

---

## 3  Push constants and batch buffers

### Batch buffers (`SwBatch.h`)

```cpp
class SwBatch {
private:
    SwGraphicsPipelineBundle* mGraphicsPipelineBundle{nullptr};

    std::vector<SwRenderItem> mRenderItems;
    SwStagingBuffer mRenderItemsStagingBuffer;
    SwAllocatedBuffer mRenderItemsBuffer;            // master, read-only lookup in both phases

    std::vector<SwRenderInstance> mRenderInstances;
    SwStagingBuffer mRenderInstancesStagingBuffer;
    SwAllocatedBuffer mRenderInstancesBuffer;        // static full candidate list

    // ---- Phase 1 (frustum) ----
    SwAllocatedBuffer mPhase1ItemCountsBuffer;       // uint per item, zeroed by CullReset
    SwAllocatedBuffer mPhase1VisibleIndexBuffer;     // read by GeometryDepthPrePass VS
    SwAllocatedBuffer mFrustumDrawItemsBuffer;       // compacted VkDrawIndexedIndirectCommand list
    SwAllocatedBuffer mFrustumDrawCountBuffer;

    // ---- Phase 1 -> Phase 2 hand-off (Option B, §4) ----
    SwAllocatedBuffer mSurvivorInstancesBuffer;      // compacted SwRenderInstance survivors
    SwAllocatedBuffer mSurvivorCountBuffer;          // uint
    SwAllocatedBuffer mOcclusionDispatchBuffer;      // VkDispatchIndirectCommand {x,y,z}

    // ---- Phase 2 (occlusion) ----
    SwAllocatedBuffer mPhase2ItemCountsBuffer;       // uint per item, zeroed by CullReset
    SwAllocatedBuffer mPhase2VisibleIndexBuffer;     // read by GeometryOpaque VS
    SwAllocatedBuffer mOcclusionDrawItemsBuffer;     // compacted draw list
    SwAllocatedBuffer mOcclusionDrawCountBuffer;
};
```

> **Per-item counts move out of `SwRenderItem`.** Today the dynamic instance count lives in
> the item struct (`mRenderInstanceCount`) and is mutated in place. To run both phases without
> a mid-frame reset, accumulate into the per-phase `…ItemCountsBuffer` arrays instead. The
> compact shaders then *assemble* the final indirect command: copy the static fields from the
> master item and set `instanceCount = mPhaseNItemCounts[itemIdx]`. `SwRenderItem` keeps its
> `mRenderInstanceCount` slot because the compacted buffer must still be a valid
> `VkDrawIndexedIndirectCommand` (it is the 2nd field; stride stays `sizeof(SwRenderItem)`).

> **Can we compact in place to save a buffer?** No. `SwCullCompact` assigns each survivor's
> destination via `InterlockedAdd`, so a survivor at source index `i` lands at `writeIndex ≤ i`.
> With threads in arbitrary order, the thread for item 100 may write slot 5 before the thread
> for item 5 has read it — a corrupting read/write race. A distinct compacted destination is
> required.

`mOcclusionDispatchBuffer` needs usage
`eIndirectBuffer | eStorageBuffer | eShaderDeviceAddress`; the survivor/count/draw buffers use
the usual storage + device-address flags.

### Push constants

`SwCullWorkFrustum`'s PC gains `mSurvivorInstancesBuffer` + `mSurvivorCountBuffer` and the
phase-1 outputs; `SwCullWorkOcclusion`'s PC takes the survivor list + count and the phase-2
outputs. The new prep PC is tiny:

```cpp
// SwCull.h  (mirror in SwCull.h.slang)
struct PrepOcclusionPC : SwPC<PrepOcclusionPC> {
    vk::DeviceAddress mSurvivorCountBuffer;     // in
    vk::DeviceAddress mOcclusionDispatchBuffer; // out
    static constexpr vk::ShaderStageFlags sStages = vk::ShaderStageFlagBits::eCompute;
};
```

---

## 4  Option B — survivor list + indirect dispatch (documented approach)

Phase 2 must iterate only the frustum survivors. Option B records them in a compacted list
during phase 1 and dispatches phase 2 indirectly over exactly that many threads. This is the
most efficient form (phase 2 launches no threads for culled instances) at the cost of one extra
tiny pass.

### 4a  Survivor list

Phase 1 already appends survivors to `mSurvivorInstancesBuffer` and bumps
`mSurvivorCountBuffer` (§2a). `CullReset` zeroes `mSurvivorCountBuffer` at the top of the frame.

### 4b  Building the indirect dispatch command (`CullPrepOcclusion`)

`vkCmdDispatchIndirect` reads a `VkDispatchIndirectCommand { uint x, y, z }` from a buffer — it
does **not** derive `ceil(count / groupSize)` for you. So a one-thread prep dispatch converts
the GPU-side survivor count into workgroup dimensions.

Shader — `SwCullPrepOcclusion.comp.slang`:

```slang
import SwCull;

// Matches VkDispatchIndirectCommand
public struct DispatchIndirectCommand { public uint mX; public uint mY; public uint mZ; };

[[vk::push_constant]] SwCull::PrepOcclusionPC prepPc;

[numthreads(1, 1, 1)]
void main() {
    uint count  = prepPc.mSurvivorCountBuffer[0];
    uint groups = (count + SwCull::MAX_1D_WORKGROUP_THREADS - 1) / SwCull::MAX_1D_WORKGROUP_THREADS;
    prepPc.mOcclusionDispatchBuffer[0] = DispatchIndirectCommand(groups, 1, 1);
}
```

C++ pass registration (`SwCull.cpp`):

```cpp
// CullPrepOcclusion
staticDeps.mReadBuffers.emplace_back(&batch.getSurvivorCountBuffer(),      SwDependency::BufferDepType::ComputeStorageRead);
staticDeps.mWriteBuffers.emplace_back(&batch.getOcclusionDispatchBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
mScene.insertPass(SwPass::Type::CullPrepOcclusion, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
    cmd.bindPipeline(mResources.mPrepPipelineBundle.getBindPoint(), mResources.mPrepPipelineBundle.getRawPipeline());
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) continue;
            mResources.mPrepPushConstants.mSurvivorCountBuffer     = batch.getSurvivorCountBuffer().getDeviceAddress().value();
            mResources.mPrepPushConstants.mOcclusionDispatchBuffer = batch.getOcclusionDispatchBuffer().getDeviceAddress().value();
            cmd.pushConstants<SwCull::PrepOcclusionPC>(
                mResources.mPrepPipelineBundle.getRawLayout(), SwCull::PrepOcclusionPC::sStages, 0, mResources.mPrepPushConstants
            );
            cmd.dispatch(1, 1, 1);
        }
    }
});
```

> **Optional:** the prep write can be folded into `CullCompactFrustum` (which already runs
> after the frustum work) by guarding `if (in.mDispatchThreadID.x == 0)` and writing the
> dispatch command there, saving a pass. Kept separate here for clarity.

### 4c  The indirect dispatch (`CullWorkOcclusion`)

The occlusion pass dispatches indirectly — one call per batch reading that batch's command
buffer:

```cpp
// CullWorkOcclusion
staticDeps.mReadImages.emplace_back(&mResources.mDepthPyramidImage,        SwDependency::ImageDepType::ComputeShaderSampledRead);
staticDeps.mReadBuffers.emplace_back(&batch.getSurvivorInstancesBuffer(),  SwDependency::BufferDepType::ComputeStorageRead);
staticDeps.mReadBuffers.emplace_back(&batch.getSurvivorCountBuffer(),      SwDependency::BufferDepType::ComputeStorageRead);
staticDeps.mReadBuffers.emplace_back(&batch.getOcclusionDispatchBuffer(),  SwDependency::BufferDepType::IndirectRead);   // see note
staticDeps.mWriteBuffers.emplace_back(&batch.getPhase2ItemCountsBuffer(),  SwDependency::BufferDepType::ComputeStorageWrite);
staticDeps.mWriteBuffers.emplace_back(&batch.getPhase2VisibleIndexBuffer(),SwDependency::BufferDepType::ComputeStorageWrite);
mScene.insertPass(SwPass::Type::CullWorkOcclusion, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
    cmd.bindPipeline(mResources.mOcclusionPipelineBundle.getBindPoint(), mResources.mOcclusionPipelineBundle.getRawPipeline());
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) continue;
            cmd.bindDescriptorSets(/* depth pyramid set */ ...);
            // ... set occlusion push constants: survivor list + count, scene buffers,
            //     mDrawExtents, mDepthPyramidExtents, phase-2 output buffers ...
            cmd.pushConstants<SwCull::WorkOcclusionPC>(...);

            // x = ceil(survivorCount / MAX_1D_WORKGROUP_THREADS), y = z = 1, at offset 0
            cmd.dispatchIndirect(batch.getOcclusionDispatchBuffer().getRawBuffer(), 0);
        }
    }
});
```

> **Dependency note.** A compute *indirect* read uses the same Vulkan stage/access as a draw
> indirect read — `VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT` + `VK_ACCESS_INDIRECT_COMMAND_READ_BIT`
> — so the existing `BufferDepType::IndirectRead` is the correct dependency for
> `mOcclusionDispatchBuffer`. This is what forces `CullPrepOcclusion` (writer) to be ordered
> before `CullWorkOcclusion`. The shader still needs the `i >= survivorCount` guard (§0.5)
> because the dispatch is rounded up to whole workgroups.

### Alternative — per-instance flag (Option A)

If you prefer to avoid the indirect dispatch and prep pass: phase 1 writes a per-instance
`uint frustumPass[i]` flag instead of a compacted list; phase 2 keeps a direct dispatch over
the full instance list and early-outs `if (!frustumPass[i]) return;` before the occlusion test.
Simpler (no `dispatchIndirect`, no prep pass, instance buffer stays read-only), but phase 2
still launches a thread per candidate. Prefer Option A for a first cut and Option B when
frustum culling removes a large fraction of instances.

---

## 5  Depth pre-pass pipeline (`SwGeometry`)

### 5a  Pipeline

Add `SwGraphicsPipelineBundle mDepthPrePassPipelineBundle` to `SwGeometry::Resources`.

| Property                | Value                                            |
| ----------------------- | ------------------------------------------------ |
| Fragment shader         | none for opaque / alpha-test FS for masked (§5b) |
| Colour attachment count | 0                                                |
| Depth write             | **on**                                           |
| Depth compare op        | **`eGreaterOrEqual`** (reverse-Z, §0.1)          |
| Colour blend            | disabled                                         |

Reuses the existing `SwGeometryWork.vert`. Depth attachment load op `eLoad` (cleared to far by
`ClearImages`), store op `eStore`. Pass an empty colour-attachment proxy to
`generateRenderingInfo`.

### 5b  Masked geometry

Opaque draws can use a no-fragment pipeline; alpha-masked draws need a fragment shader that
samples the diffuse texture and `discard`s below threshold, or their silhouettes in the depth
buffer (and therefore in the pyramid and the `eEqual` colour pass) are wrong. Recommended: one
pre-pass pipeline with an alpha-test FS shared by opaque + masked, branching on a material
flag, so the whole opaque+mask set lands in depth and the colour pass uses `eEqual` uniformly.

### 5c  Pass registration (`GeometryDepthPrePass`)

Draws indirect from `mFrustumDrawItemsBuffer` + `mFrustumDrawCountBuffer` for Opaque and Mask
batch types.

- **Writes:** depth image (`DepthAttachmentReadWrite`).
- **Reads:** depth image; vertex / index / instance / node-transform / material buffers; the
  per-frame buffer; `mPhase1VisibleIndexBuffer` (`VertexShaderStorageRead`);
  `mFrustumDrawItemsBuffer` (`IndirectRead`); `mFrustumDrawCountBuffer`.

---

## 6  Update `GeometryOpaque` main pass

| Property         | Change                                       |
| ---------------- | -------------------------------------------- |
| Depth compare op | **`eEqual`** (was `eGreaterOrEqual`)         |
| Depth write      | **off** (depth already correct)              |
| Depth load op    | `eLoad` — already the default; no change     |

`GeometryOpaque` draws `mOcclusionDrawItemsBuffer` and reads `mPhase2VisibleIndexBuffer`.
`GeometryTransparent` is unchanged.

---

## 7  Pass ordering and dependencies (`SwScene::draw`)

```cpp
mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
if (!mCull.getFreeze()) {
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryDepthPrePass]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullDepthPyramid]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPrepOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactOcclusion]);
}
// ... Pick / Skybox as today ...
mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryOpaque]);
mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
mRenderGraph.addPass(&mPasses[SwPass::Type::WBOITComposite]);
mRenderGraph.addPass(&mPasses[SwPass::Type::CopyToSwapchain]);
mRenderGraph.addPass(&mPasses[SwPass::Type::Gui]);
```

Hazard chains the graph must resolve (all serialised by declared deps):

- **survivor list / count:** CullReset (zero count) → CullWorkFrustum (append) →
  CullPrepOcclusion (read count → write dispatch cmd) **and** CullWorkOcclusion (read list).
- **`mOcclusionDispatchBuffer`:** CullPrepOcclusion (write) → CullWorkOcclusion (`IndirectRead`).
- **phase-1 buffers:** CullReset (zero) → CullWorkFrustum (write) → CullCompactFrustum (read
  counts → write draw list) → GeometryDepthPrePass (vertex + indirect read). Untouched after.
- **phase-2 buffers:** CullReset (zero) → CullWorkOcclusion (write) → CullCompactOcclusion
  (read counts → write draw list) → GeometryOpaque (vertex + indirect read). Untouched before.
- **depth image:** ClearImages (clear far) → GeometryDepthPrePass (depth write) →
  CullDepthPyramid (sampled read) → GeometryOpaque (`eEqual`, no write) → GeometryTransparent.

Because the phases own disjoint buffers, there is no inter-phase WAR/WAW on the cull scratch —
the only phase-1 → phase-2 edge is the survivor list and the dispatch command.

When culling is frozen (`mCull.getFreeze()`), none of the cull/pre-pass passes run and
`GeometryOpaque` relies on the previously populated depth + occlusion draw list; confirm this
matches the intended freeze semantics (a frozen frame reuses the last depth buffer, so it may
be preferable to keep `eGreaterOrEqual`/write-on on frozen frames).
