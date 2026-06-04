# Depth Pre-Pass Implementation Plan

## Overview

A depth pre-pass renders all opaque geometry z-only before the main colour pass. This enables
same-frame HiZ occlusion culling (no temporal lag) and eliminates overdraw in the main pass.

### Target frame sequence

```
ClearImages         — clear colour + depth
CullReset           — reset instance counts
CullWorkFrustum     — frustum cull only → phase-1 compact list
CullCompactPhase1   — write indirect draw commands for pre-pass
GeometryDepthPrePass— z-only draw of frustum-visible geometry
CullDepthPyramid    — build HiZ from current-frame depth
CullWorkOcclusion   — occlusion cull phase-1 list → phase-2 compact list
CullCompactPhase2   — write indirect draw commands for main pass
GeometryOpaque      — full shading, depth compare = eEqual, depth write off
GeometryTransparent — unchanged
...
```

---

## 1  Revert recent changes

The depth clear must stay in `ClearImages` (its original location). Revert:
- `SwScene.cpp` — restore depth image dependency and `eClear` attachment in `ClearImages`
- `SwGeometry.cpp` — restore depth attachment to `eLoad` in `GeometryOpaque`

---

## 2  Add new pass types (`SwPass.h`)

```cpp
enum class Type {
    ClearImages,
    CullReset,
    CullWorkFrustum,       // new — frustum only
    CullCompactPhase1,     // new
    GeometryDepthPrePass,  // new
    CullDepthPyramid,
    CullWorkOcclusion,     // new — occlusion only (was CullWork)
    CullCompactPhase2,     // new (was CullCompact)
    ...
};
```

Rename the existing `CullWork` → `CullWorkOcclusion` and `CullCompact` → `CullCompactPhase2`
throughout the codebase.

---

## 3  Split `CullWork` into two passes (`SwCull`)

### 3a  Shader (`SwCullWork.comp.slang`)

Add a push-constant flag `mFrustumOnly` to `WorkPC` (both C++ and Slang structs).
When true, skip the `occlusionCull` call and always pass if frustum-visible.

```slang
bool visible = frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform);
if (visible && !workPc.mFrustumOnly)
    visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents);
```

### 3b  C++ (`SwCull.cpp / SwCull.h`)

- Add `bool mFrustumOnly` to `WorkPC` and set it appropriately per pass.
- Register two passes — `CullWorkFrustum` and `CullWorkOcclusion` — both using the same
  pipeline but with different push-constant values.
- `CullWorkFrustum` does **not** declare the depth pyramid image as a dependency.
- `CullWorkOcclusion` declares it as `ComputeShaderSampledRead` (unchanged from current).

---

## 4  Add phase-1 buffers to `SwBatch`

Each batch currently has one pre/post-cull pair. It needs a second post-cull pair for
the two-phase output:

| Buffer | Purpose |
|---|---|
| `mPreCullRenderItemsBuffer` | written by Reset, read by both cull passes |
| `mPostCullRenderItemsBufferPhase1` | compact output for depth pre-pass |
| `mPostCullRenderItemsCountBufferPhase1` | count for phase-1 |
| `mPostCullRenderItemsBuffer` (existing) | compact output for main pass |
| `mPostCullRenderItemsCountBuffer` (existing) | count for main pass |

Update `CullReset` to also fill/reset the new phase-1 buffers.
Update `CullCompactPhase1` to write into phase-1 buffers.
`CullCompactPhase2` (renamed from `CullCompact`) writes into the existing phase-2 buffers
using the pre-cull items that survived both cull passes.

---

## 5  Depth pre-pass pipeline (`SwGeometry`)

### 5a  Pipeline

Create a new `SwGraphicsPipelineBundle mDepthPrePassPipelineBundle` in `SwGeometry::Resources`.

Key differences from the opaque pipeline:

| Property | Value |
|---|---|
| Fragment shader | none (or passthrough for masked — see §5b) |
| Colour attachment count | 0 |
| Depth write | on |
| Depth compare op | `eLessOrEqual` |
| Colour blend | disabled |

The vertex shader is the same as the opaque geometry shader (needs world-space position).

### 5b  Masked geometry

Opaque geometry can use a no-fragment pipeline. Masked (alpha-tested) geometry must run a
fragment shader that samples the diffuse texture and discards fragments below the alpha
threshold, otherwise geometry silhouettes will be incorrect in the depth buffer.

Options:
- One pipeline per material type in the pre-pass (opaque: no FS, masked: alpha-test FS).
- Or: combine into one pipeline with an alpha-test FS and branch on a push constant; the
  cost on opaque draws is negligible.

### 5c  Pass registration

`GeometryDepthPrePass` dispatches indirect draws from `mPostCullRenderItemsBufferPhase1`.
Dependencies:
- Writes: depth image (`DepthAttachmentReadWrite`)
- Reads: vertex/index/instance buffers, phase-1 count+item buffers

---

## 6  Update `GeometryOpaque` main pass

With the pre-pass having already established depth, the main pass can skip re-drawing hidden
fragments:

| Property | Change |
|---|---|
| Depth compare op | `eEqual` (was `eLessOrEqual`) |
| Depth write | off (depth is already correct) |
| Depth attachment load op | `eLoad` (reads pre-pass depth) |

The main pass uses `mPostCullRenderItemsBuffer` (phase-2, occlusion-culled).

---

## 7  Restore depth clear to `ClearImages`

`ClearImages` clears depth with `eClear` as before. `GeometryDepthPrePass` then populates it
with `eLoad` + `eLessOrEqual` writes. `CullDepthPyramid` runs after the pre-pass and reads
a fully populated, current-frame depth buffer — no temporal lag.

---

## 8  Pass ordering in `SwScene::render`

```cpp
mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
if (!mCull.getFreeze()) {
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactPhase1]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryDepthPrePass]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullDepthPyramid]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactPhase2]);
}
mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryOpaque]);
mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
...
```

---

## 9  Summary of files to touch

| File | Change |
|---|---|
| `SwPass.h` | Add new pass types |
| `SwBatch.h / .cpp` | Add phase-1 post-cull buffers |
| `SwCull.h` | Add `mFrustumOnly` to `WorkPC`, new Resources entries |
| `SwCull.cpp` | Register `CullWorkFrustum`, `CullCompactPhase1`, rename existing passes |
| `SwCullWork.comp.slang` | Add `mFrustumOnly` flag, skip occlusion branch |
| `SwCull.h.slang` | Add `mFrustumOnly` to `WorkPC` struct |
| `SwGeometry.h / .cpp` | Add pre-pass pipeline, register `GeometryDepthPrePass` pass |
| `SwGeometry.vert.slang` | No change expected |
| `SwGeometryDepthPrePass.frag.slang` | New — passthrough or alpha-test only |
| `SwScene.cpp` | Revert depth clear, update pass ordering |
