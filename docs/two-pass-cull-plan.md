# Two-Pass Occlusion Culling Plan

Nanite-style temporal two-pass cull. Early pass draws last-frame-visible instances and writes
real depth. Late pass tests the remainder against that same-frame Hi-Z.

---

## Motivation

The current single-pass cull builds Hi-Z from **last frame's** depth, so occlusion always lags
one frame. Newly visible objects (e.g. moving out from behind an occluder) are not drawn until
the frame after they become visible.

With two passes the early geometry draw produces a same-frame depth buffer. The late cull tests
against *that*, so the lag is eliminated for newly visible objects while the safe set is still
drawn efficiently.

---

## Pass Sequence

```
Current:
  CullReset → CullPrepOcclusion → CullWork → CullCompact
  → GeometryOpaque → GeometryTransparent

New:
  CullReset
  → CullPrepOcclusion          (Hi-Z from frame N-1 depth, same as now)
  → CullEarlyWork              (frustum + lastFrameVisible flag → early draw list)
  → CullEarlyCompact
  → GeometryEarlyOpaque        (full shade + depth write for early set)
  → CullPrepOcclusionSameFrame (Hi-Z from the depth just written above)
  → CullLateWork               (frustum + same-frame occlusion → late draw list)
  → CullLateCompact
  → GeometryLateOpaque
  → GeometryTransparent
```

`GeometryEarlyOpaque` is a full shaded draw (color + depth), not a depth-only prepass. Its sole
purpose is populating the depth buffer so the same-frame Hi-Z is valid. The color output is a
free bonus.

---

## Data Changes

### SwScene — visibility buffers

Add two `SwAllocatedBuffer` to `SwScene`, each `uint32_t[sceneInstanceCount]`. Ping-pong them
each frame (swap which is "read" and which is "write" at the top of CullReset).

```cpp
SwAllocatedBuffer mVisibilityBufA;   // uint32_t per scene instance, 0 = invisible
SwAllocatedBuffer mVisibilityBufB;
uint32_t mVisibilityReadIndex{0};    // 0 → A is read (prev frame), B is write (this frame)
```

`mVisibilityRead`  — prev frame's results; read by CullEarlyWork to identify the safe set.  
`mVisibilityWrite` — zeroed in CullReset; written by CullEarlyWork and CullLateWork.

Sizing: must have enough capacity for number of render instances. Reallocate (and re-register dynamic deps) whenever
the scene instance count changes, following the same `SwBufferFactory` generation pattern used
elsewhere.

Cold start (frame 0): `mVisibilityRead` is all zeros → early set is empty → late pass draws
everything. Correct by construction.

### SwBatch — early draw buffers

Add alongside the existing occlusion buffers:

```cpp
SwAllocatedBuffer mEarlyRItemsBuffer;   // SwRenderItem[], compacted early draw list
SwAllocatedBuffer mEarlyRItemsCount;    // uint32_t, count of compacted early items
```

The existing `mOcclusionRItemsBuffer` / `mOcclusionRItemsCount` become the **late** draw list
(rename optional but clarifying).

Accessors to add to `SwBatch`:

```cpp
inline SwAllocatedBuffer& getEarlyRItemsBuffer() { return mEarlyRItemsBuffer; }
inline SwAllocatedBuffer& getEarlyRItemsCount()  { return mEarlyRItemsCount; }
```

`getFinalRItemsBuffer()` / `getFinalRItemsCount()` should point to the late buffer (last in the
chain), which they already do via `mOcclusionRItemsBuffer`.

### SwPass::Type — new entries

```cpp
CullEarlyWork,
CullEarlyCompact,
CullLateWork,
CullLateCompact,
GeometryEarlyOpaque,
GeometryLateOpaque,
```

Remove `CullWork`, `CullCompact`, `GeometryOpaque` (or keep `GeometryOpaque` as an alias for
`GeometryLateOpaque` to minimise churn elsewhere).

### WorkPC — new fields

Both `CullEarlyWork` and `CullLateWork` need two extra device addresses:

```cpp
vk::DeviceAddress mVisibilityReadBuffer;   // prev frame, read-only
vk::DeviceAddress mVisibilityWriteBuffer;  // this frame, write
```

Add to `SwCull::WorkPC` (both CPU struct in `SwCull.h` and Slang struct in `SwCull.h.slang`).

---

## Shader Changes

### SwCullReset.comp.slang

Add: zero `mVisibilityWriteBuffer` (not read — it must survive from last frame).

```slang
// existing: fillBuffer for RInstsScratchCount, RItemCounts, etc.
// new:
memset(workPc.mVisibilityWriteBuffer, 0, workPc.mSceneInstancesLimit * sizeof(uint));
```

In practice this is done CPU-side with `cmd.fillBuffer(visibilityWriteBuf, 0, wholeSize, 0)`
in the CullReset pass callback, same as the existing fills.

### SwCullEarlyWork.comp.slang  *(new)*

Copy of `SwCullWork.comp.slang`. Change `main()`:

```slang
uint instIdx = rInst.mSceneInstanceIndex;
bool lastVisible = workPc.mVisibilityReadBuffer[instIdx] != 0;

// Early pass: only draw what was visible last frame
bool visible = frustumTest(frustum, aabbCorners) && lastVisible;
if (!visible) return;

// Mark visible this frame
InterlockedOr(workPc.mVisibilityWriteBuffer[instIdx], 1u);

// Write to early visible-instances list (same atomics as current CullWork)
uint _;
InterlockedAdd(*workPc.mRInstsCount, 1, _);
uint offset;
InterlockedAdd(rItem->mRInstCount, 1, offset);
workPc.mSceneVisibleRInstsIndicesBuffer[rItem->mFirstRInst + offset] = instIdx;
```

No occlusion test — the early set is trusted from last frame's visibility.

### SwCullLateWork.comp.slang  *(new)*

Copy of `SwCullWork.comp.slang`. Change `main()`:

```slang
uint instIdx = rInst.mSceneInstanceIndex;
bool lastVisible = workPc.mVisibilityReadBuffer[instIdx] != 0;

// Late pass: skip anything already handled by the early pass
if (lastVisible) return;

bool visible = frustumTest(frustum, aabbCorners);
if (visible) visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, workPc.mDrawExtents);
if (!visible) return;

InterlockedOr(workPc.mVisibilityWriteBuffer[instIdx], 1u);

uint _;
InterlockedAdd(*workPc.mRInstsCount, 1, _);
uint offset;
InterlockedAdd(rItem->mRInstCount, 1, offset);
workPc.mSceneVisibleRInstsIndicesBuffer[rItem->mFirstRInst + offset] = instIdx;
```

`occlusionCull()` is unchanged — it samples the same `depthPyramidImage` descriptor, which at
this point is bound to the same-frame Hi-Z built after `GeometryEarlyOpaque`.

The compact shaders (`SwCullCompact`) are reused as-is for both early and late, pointed at the
appropriate source/dest buffers.

---

## SwCull::System Changes

### initializePasses()

Replace the single `CullWork` + `CullCompact` + `CullPublishCount` sequence with:

```
CullReset           — unchanged, plus fillBuffer for mVisibilityWrite
CullPrepOcclusion   — unchanged (reads last frame's depth)
CullEarlyWork       — new pipeline; WorkPC with mVisibilityRead/Write
CullEarlyCompact    — reuse compact pipeline; src=InitialRItems, dst=EarlyRItems
CullPublishCount    — keep here or move to end (counts early visible set)
[geometry passes registered by SwGeometry]
CullPrepOcclusionSameFrame — second Hi-Z build; same pipeline/descriptor, but
                              depth already has early geometry in it
CullLateWork        — new pipeline; WorkPC with mVisibilityRead/Write
CullLateCompact     — reuse compact pipeline; src=InitialRItems, dst=LateRItems
CullPublishCount    — second publish (or accumulate into same counter)
```

`CullPrepOcclusionSameFrame` is the same compute pipeline as `CullPrepOcclusion` with the same
descriptor set. No new resources needed; just a second registered pass of type
`CullPrepOcclusionSameFrame` so the render graph can order it correctly.

### refreshDynamicDependencies()

Add dynamic deps for the new passes:

- `CullEarlyWork` write: `mEarlyRItemsBuffer` (per batch)
- `CullEarlyCompact` read/write: `InitialRItems` / `EarlyRItems`
- `CullLateWork` write: `LateRItemsBuffer` (per batch)
- `CullLateCompact` read/write: `InitialRItems` / `LateRItems`
- Both Work passes read: `mVisibilityReadBuffer`; write: `mVisibilityWriteBuffer`

### Visibility buffer swap

At the start of each frame (before CullReset executes), swap read/write:

```cpp
mVisibilityReadIndex ^= 1;
// then refresh push constants so WorkPC addresses point to the new read/write sides
```

This can live in `SwCull::System::refreshPushConstants()`.

---

## SwGeometry::System Changes

Add `initializeEarlyPass()` and `initializeLatePass()` (or parameterise the existing
`initializePass()`) to register `GeometryEarlyOpaque` and `GeometryLateOpaque`.

`GeometryEarlyOpaque` uses `mEarlyRItemsBuffer` / `mEarlyRItemsCount` for indirect draws.  
`GeometryLateOpaque` uses `mLateRItemsBuffer` / `mLateRItemsCount`.

Both use the same `WorkPC`, vertex shader, and fragment shader as the current `GeometryOpaque`.
The only difference is which draw-RItems buffer is passed via push constants.

---

## Footguns

| Footgun                                                                                                                                                                                                  | Mitigation                                                                                                                                              |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `CullPrepOcclusionSameFrame` must run **after** `GeometryEarlyOpaque` and **before** `CullLateWork`. If the pass order is wrong the same-frame Hi-Z reads cleared or stale depth.                        | Register `GeometryEarlyOpaque` in `SwGeometry` before the late cull passes in `SwCull`. Verify topological sort output.                                 |
| Opaque pipeline depth state: both `GeometryEarlyOpaque` and `GeometryLateOpaque` must have **depth write ON, compare `eGreaterOrEqual`** (reverse-Z).                                                    | Same as the footgun from the prepass revert — do not set `eEqual` or write-off.                                                                         |
| Visibility buffer size must match `sceneInstanceCount`. Stale size after scene load = out-of-bounds writes.                                                                                              | Tie reallocation to the same scene-dirty flag that reallocates `mSceneInstancesBuffer`.                                                                 |
| On frame 0, `mVisibilityRead` is all zeros → early set is empty → late pass draws everything against a cleared Hi-Z → all instances visible. This is correct but means frame 0 has no occlusion culling. | Acceptable. Alternatively seed the buffer to all-ones to aggressively early-draw on frame 1, but this risks overdraw if scene is large. Leave as zeros. |
| If the camera or scene is frozen (`mFreeze`), the visibility buffer must still be swapped to avoid reading this-frame data as last-frame.                                                                | The swap happens unconditionally in `refreshPushConstants`, independent of `mFreeze`.                                                                   |

---

## Implementation Order

1. Add `mVisibilityBufA/B` to `SwScene`. Wire CullReset to zero the write side.
2. Add `mEarlyRItemsBuffer/Count` to `SwBatch`.
3. Add new `SwPass::Type` entries.
4. Write `SwCullEarlyWork.comp.slang` and compile to `.spv`.
5. Register `CullEarlyWork` + `CullEarlyCompact` in `SwCull::System`.
6. Register `GeometryEarlyOpaque` in `SwGeometry::System`.
7. Register `CullPrepOcclusionSameFrame` (second Hi-Z build pass) in `SwCull::System`.
8. Write `SwCullLateWork.comp.slang` and compile to `.spv`.
9. Register `CullLateWork` + `CullLateCompact` in `SwCull::System`.
10. Register `GeometryLateOpaque` in `SwGeometry::System`.
11. Remove old `CullWork`, `CullCompact`, `GeometryOpaque` passes.
12. Wire visibility buffer ping-pong in `refreshPushConstants`.
13. Verify pass order via render graph debug output; confirm no validation errors.
