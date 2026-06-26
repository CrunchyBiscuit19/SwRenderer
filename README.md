# SwRenderer

A Vulkan 1.4 renderer written in C++23, targeting GPU-driven rendering with a render-graph architecture. Built primarily as a learning project and testbed for modern rendering techniques.

---
## Features

### Rendering Pipeline

- GPU-Driven Indirect Drawing + Bindless Techniques (Descriptor Indexing, Buffer Device Address).
- Render graph with automatic Vulkan barrier insertion.
- Physically-Based Rendering (PBR) with Cook-Torrance microfacet and Image-Based Lighting (IBL).
- Weighted Blended Order-Independent Transparency (WBOIT).
- Two-Pass Hierarchical Z-Buffer Frustum + Occlusion Culling.
- Fast Approximated Anti-Aliasing (FXAA).
- ACES Filmic Tone Mapping.
- Mouse picking + ImGuizmo instance manipulation.

### Asset Support

- GLTF 2.0 Scene Loading
- Runtime asset loading via in-engine file browser

---

## Vulkan Handle Abstractions

* Most Vulkan handles are wrapped in RAII classes to automatically manage lifttimes.
* Prefixed by `Sw`.
* Labelled by given name in debug builds with `VulkanDebug::setObjectName(...)`.

### Buffers `SwBuffer`

* Manages both the `VkBuffer` and its VMA allocation.
* Split into general purpose `SwAllocatedBuffer` and specialized `SwStagingBuffer` (CPU-visible, persistently mapped).
* Supports bindless buffer access via `VkBufferDeviceAddress` and `SwBuffer::getDeviceAddress()`.
* Tracks the flags, usage, size, pipeline stage, and access bits.
* Convenience methods for barrier insertion.
* Copy methods handle resize and synchronization automatically.
* Created via `SwBufferFactory`, which manages a shared staging buffer for uploads.

### Images `SwImage`

* Abstract base `SwImage` over the concrete `SwSwapchainImage` (wraps a swapchain-owned `VkImage`) and `SwAllocatedImage` (owns a `VkImage` + VMA allocation).
* `SwAllocatedImage` is specialized into `SwColorImage2D`, `SwDepthImage2D`, and `SwColorImageCubemap`.
* Tracks the current layout, pipeline stage, and access bits; convenience methods for barrier insertion and layout transitions (including via `SwDependency` types).
* Holds a main image view plus optional additional views/formats; supports mipmap generation and resizing.
* `SwImageFactory` creates images (uploading pixel data through a shared staging buffer), builds image views, and provides a set of default fallback textures (white, grey, black, blue, checkerboard).

### Samplers `SwSampler`

* Wraps a `VkSampler`.
* `SwSamplerOptions` captures the sampler state (filters, mipmap mode, address modes) and is hashable, so `SwSamplerFactory` can de-duplicate identical samplers.
* Exposes a shared `sDefaultSampler` and a query for the device's max anisotropy.

### Pipelines `SwPipelineBundle`

* `SwPipelineLayout` wraps a `VkPipelineLayout`; `SwPipelineBundle` pairs a `VkPipeline` with its layout and a unique auto-assigned id.
* Specialized into `SwGraphicsPipelineBundle` and `SwComputePipelineBundle`, each reporting its bind point.
* `SwGraphicsPipelineFactory` / `SwComputePipelineFactory` build pipelines from an options struct (shaders + entry points, topology, raster/cull state, blending, depth state, etc.), and `SwPipelineFactory` builds pipeline layouts from descriptor set layouts and push-constant ranges.

### Descriptors `SwDescriptorSet`

* `SwDescriptorLayout` wraps a `VkDescriptorSetLayout` with its bindings and a bindless flag.
* `SwDescriptorSet` wraps a `VkDescriptorSet` and accumulates image/sampler/buffer writes, flushed with `pushWrites()`.
* `SwDescriptorAllocator` manages a growing pool of `SwDescriptorPool`s (by `SwPoolSizeRatio`), creating layouts and sets and recycling pools; supports bindless descriptor counts.

### Push Constants `SwPC`

* `SwPC<T>` is a small templated helper that derives a `VkPushConstantRange` from a push-constant struct `T`, using `T::sStages` and `sizeof(T)`.

### Shaders `SwShader`

* Wraps a `VkShaderModule` plus its `vk::ShaderStageFlagBits`.
* `SwShaderFactory` creates a shader module from a compiled SPIR-V file path and a shader stage.

### Semaphores and Fences `SwSemaphore` / `SwFence`

* `SwSemaphore` wraps a `VkSemaphore` and can produce a `vk::SemaphoreSubmitInfo` for a given stage mask.
* `SwFence` wraps a `VkFence` with a `reset()` helper.
* `SwSemaphoreFactory` / `SwFenceFactory` create them by name (fences with create flags).

### Command Buffers and Command Pools `SwCommandBuffer` / `SwCommandPool`

* `SwCommandPool` wraps a `VkCommandPool`; `SwCommandBuffer` wraps a `VkCommandBuffer` with `reset` / `begin` / `end` helpers and a `generateSubmitInfo()`.
* `SwCommandBufferFactory` allocates command buffers from either a `SwCommandPool` or a raw `VkCommandPool`; `SwCommandPoolFactory` creates pools with the given create flags.

---

## Data Representation

* Data abstractions to represent the scene and its assets for the renderer to draw.
* Ownership flows from the **asset** outward: a `SwAsset` owns its meshes, materials, nodes, lights, and instances. References between types are by C++ reference/pointer on the CPU side and by buffer index on the GPU side. See [Scene Data Model](#scene-data-model) for how these flatten into GPU buffers.

### Camera

* **`SwCamera`** — a movable camera supporting free-fly and drone movement modes (`SwMovementMode`), driven by SDL events. Holds position/pitch/yaw/speed, computes the six frustum planes, and keeps them in a GPU buffer (`SwCull::Plane`) for the cull pass. Offers a spawn transform helper for placing new instances in front of the camera.
* **`SwPerspective`** — a view + projection matrix pair (reversed-Z, Y-flipped Vulkan projection); produced by `SwCamera::getPerspective()`.

### Asset

* **`SwAsset`** — everything parsed from one glTF file: its meshes, materials, images/samplers, node hierarchy, lights, per-mesh bounds, and instances. Owns the asset's GPU buffers (material constants, node transforms, instances, bounds), built from the raw `fastgltf::Asset`.
* **Relations** — the asset is the root container: it holds `SwMesh`es, `SwMaterial`s, `SwNode`s (including `SwMeshNode`/`SwLightNode`), `SwLight`s, and `SwInstance`s. It is assigned contiguous base offsets into the scene-wide buffers (`mFirstMaterialInScene`, `mFirstNodeTransformInScene`, `mFirstInstanceInScene`, `mFirstBoundInScene`) to convert its children's *relative* indices to *scene-absolute* ones. `createInstance(...)` adds a placement; `generateRcsAndRis()` walks the nodes to emit draw data.

### Material

* **`SwMaterial`** — a PBR material; classified `Opaque` / `Mask` / `Transparent` from the glTF alpha mode.
* **`SwMaterialConstants`** — the scalar/vector factors (base, emissive, metallic-roughness, normal scale, occlusion strength, alpha cutoff).
* **`SwMaterialResources`** — the five PBR textures (base, metallic-roughness, normal, occlusion, emissive).
* **`SwMaterialTexture`** — pairs a `SwColorImage2D` with a `SwSampler`, with default white/error fallbacks.
* **`SwMaterialPipelineOptions`** — double-sided + alpha mode; used as a hash key to share/de-duplicate graphics pipeline bundles across materials.
* **Relations** — a `SwMaterial` bundles one `SwMaterialConstants` and one `SwMaterialResources` (which holds five `SwMaterialTexture`s, each wrapping an image + sampler). Materials are referenced by `SwPrimitive`; the alpha-mode classification decides which material-type batch a primitive lands in.

### Instance

* **`SwInstance`** — a placement of an entire asset in the world, holding the owning asset id and a transform (`SwInstance::Data`); can be flagged for deletion.
* **`SwInstance::Data`** — the GPU-facing payload, currently just `mTransformMatrix`.
* **Relations** — created per placement via `SwAsset::createInstance(...)` and owned by that asset. An instance applies its transform on top of every node's local transform, so one instance effectively draws the whole node hierarchy once.

### Node

* **`SwNode`** — a node in the glTF transform hierarchy: a named local/world transform with parent and children, refreshed recursively from a parent transform.
* **`SwMeshNode`** — a node that additionally references a `SwMesh`.
* **`SwLightNode`** — a node that additionally references a `SwLight` (and its owning asset id).
* **Relations** — nodes form a parent/child tree owned by the asset. `generateRcsAndRis()` is virtual: a `SwMeshNode` emits one render command per primitive of its mesh (× instances → render items); a `SwLightNode` ties a light to its node transform for positioning.

### Mesh

* **`SwMesh`** — a drawable mesh: a list of `SwPrimitive`s plus an AABB (`SwBounds`). Owns its vertex/index buffers and tracks offsets into the scene-wide vertex/index buffers and bounds.
* **`SwPrimitive`** — the smallest drawable unit: an index range (`mRelativeFirstIndex`, `mIndexCount`, `mRelativeVertexOffset`) plus the `SwMaterial` it is drawn with.
* **`SwVertex`** — the vertex layout: position, normal, color, uv, tangent (with handedness in `w`).
* **`SwBounds`** — a min/max AABB used for frustum/occlusion culling.
* **Relations** — a mesh can mix materials, so it splits into primitives, each referencing one `SwMaterial`. A `SwMeshNode` references the mesh; the mesh's `SwBounds` feeds the cull pass.

### Light

* **`SwLight`** — a punctual light (`Directional` / `Point` / `Spot`).
* **`SwLight::Params`** — author-facing parameters (color, intensity, range, cone angles).
* **`SwLight::Data`** — the GPU-packed form (via `toData(...)`), which also stores indices into the scene node-transform and instance buffers for positioning.
* **`SwSunlight`** — a separate, simpler directional sun (intensity, azimuth/elevation, color).
* **Relations** — a `SwLight` is referenced by a `SwLightNode`, which supplies the node transform / instance indices baked into `SwLight::Data`.

### Batch

* **`SwBatch`** — groups all render commands and render items that share one graphics pipeline; owns the staging + GPU buffers backing the indirect draw lists (including the compacted early/late command buffers and counts used by the two-pass cull).
* **`SwRenderCommand`** — one per mesh-node primitive; its first fields mirror `VkDrawIndexedIndirectCommand`, plus indices into the scene-wide material/node-transform/bounds/instance buffers.
* **`SwRenderItem`** — one per (render command × instance); just a render-command index and a scene instance index. It is the unit of culling.
* **Relations** — batches are keyed by pipeline id and live in per-material-type maps on the scene. A `SwRenderItem` points back to its `SwRenderCommand` (same batch) and to a `SwInstance`; a `SwRenderCommand` indexes the scene-wide material, node-transform, bounds, and instance buffers.

---

## Scene and Systems

* A system is a collection of Vulkan resources and related functionality to contribute to the drawing of the scene. 
* Each system lives in its own `SwX` namespace and follows the same shape: a `Resources` struct (pipelines, descriptors, images, push constants), zero or more `SwPC`-derived push-constant structs, and a `System` class.
* The scene controls the systems and assets, and orchestrates the rendering each frame by calling into the systems in order.
* The scene also owns and manages the "global" buffers and descriptor arrays that will be used to draw the final image.

### System `SwSystem`

* **`SwSystem`** — the base class every system's `System` derives from. Holds a reference to the owning `SwScene` and defines the lifecycle hooks: `initializeResources()` / `initializePasses()` / `initializePushConstants()`, plus per-frame `refreshDynamicDependencies()` / `refreshPushConstants()` / `refresh()`.
* **`SwSystem::Resizable`** — mix-in for systems whose resources depend on the swapchain size; provides `resize()` driving a `reInitializeOnResize()` override.

### Scene `SwScene`

* **`SwScene`** — owns the camera, the loaded assets (`mAssets`), the per-material-type batch maps (`mBatchTypes`), the passes (`mPasses`), the render graph, and every system instance (`SwCull`, `SwPick`, `SwIBL`, `SwWBOIT`, `SwGeometry`, `SwPostProcess`, `SwLighting`, `SwGui`).
* **`SwScene::Flags`** — per-frame change flags (asset/instance loaded/unloaded, buffer reload requests) that drive what gets rebuilt.
* Owns the scene-wide "global" GPU buffers (vertex, index, material constants, node transforms, instances, bounds, draw-RI indices, lights, plus a double-buffered visibility-RI buffer for occlusion) and the bindless material-resources descriptor set.
* **Relations** — `loadAssets(...)` parses assets and `regenerateRcsAndRis()` rebuilds the batches; the `realign*` / `reloadScene*Buffer` helpers repack the global buffers when assets change. `draw()` refreshes systems and executes the render graph each frame. Systems are friends of the scene so they can read its buffers.

### Passes and Dependencies `SwPass` / `SwDependency`

* **`SwPass`** — one unit of GPU work: a `SwPass::Type` (the fixed enum of all passes, e.g. `CullEarlyWork`, `GeometryLateOpaque`, `Tonemap`, `Gui`), a command-buffer callback, static + dynamic `SwDependency`s, and `mustRun` / `pruned` flags. Provides helpers for dynamic-rendering info and viewport/scissor setup.
* **`SwDependency`** — the read/write image and buffer dependencies of a pass. `ImageDep` / `BufferDep` pair a resource with an `ImageDepDesc` / `BufferDepDesc` (stage + access + layout), derived from the high-level `ImageDepType` / `BufferDepType` enums (e.g. `ColorAttachmentReadWrite`, `ComputeStorageRead`, `IndirectRead`).
* **Relations** — passes are created by systems (via `SwScene::insertPass(...)`) and consumed by the render graph; their declared dependencies are what the graph uses to insert barriers and prune.

### Render Graph `SwRenderGraph`

* **`SwRenderGraph`** — collects passes and output images, prunes passes that don't contribute to an output, topologically sorts the rest (`compile()`), and `execute()`s them on a command buffer — inserting the Vulkan barriers implied by each pass's `SwDependency`s.
* Can export the compiled graph to Graphviz for inspection (`requestRenderGraph()` / `exportRenderGraph()`).
* **Relations** — owned by the `SwScene`; operates on the `SwPass`es the systems register.

### Cull `SwCull`

* Two-pass GPU frustum + Hierarchical-Z occlusion cull; one compute thread per render item.
* **`Plane`** — a frustum plane (used by `SwCamera`); **`Phase`** — `Early` / `Late`.
* **`ResetPC` / `WorkPC` / `CompactPC` / `PrepOcclusionPC`** — compute push constants for the reset, cull-work, compaction, and depth-pyramid-build stages (mostly buffer device addresses).
* **`Resources`** — the reset/work/compact/prep-occlusion compute pipelines and their layouts/descriptors, plus the depth-pyramid image and samplers.
* **`System`** (`SwSystem::Resizable`) — registers the cull passes; can be frozen for debugging. **Relations** — produces the indirect draw lists consumed by `SwGeometry`; reads the scene bounds/transforms/instances buffers. See [Scene Data Model](#scene-data-model).

### Lighting `SwLighting`

* **`AssetLight`** — a per-instance binding emitted by asset `SwLightNode`s during regen: a non-owning pointer to the asset-owned `SwLight`, its node-transform and instance indices, and the cached world position and direction. The system references the light object rather than copying its `SwLight::Data`.
* **`Resources`** — the scene's lights: a single global `SwSunlight` and the `AssetLight` references. All punctual lights now belong to assets, so there is no separate editor/global light list.
* **`System`** — flattens its `AssetLight` references into `SwLight::Data` via `collectLightData()`; the scene packs that into `mSceneLightsBuffer` (`reloadSceneLightsBuffer`).
* **Relations** — feeds the light buffer that `SwGeometry`'s shaders consume; the sunlight is uploaded per frame via the per-frame buffer.

### Image-Based Lighting `SwIBL`

* Bakes the IBL maps from an environment equirect and draws the skybox.
* **`PrefilterPC` / `DrawPC`** — push constants for the specular prefilter bake and the skybox draw.
* **`Resources`** — the baked irradiance / specular-prefilter (mip-chained) / BRDF-LUT images and their samplers, the bake compute pipelines, the consume descriptor set, and the skybox cube vertex buffer + draw pipeline.
* **`System`** — bakes on environment load/change, owns the skybox draw pass, and exposes a GUI-controlled IBL intensity. The static `sConsumeDescriptorLayout` (set-1) is created before `SwMaterial::init`.
* **Relations** — `sConsumeDescriptorLayout` is referenced by `SwMaterial`'s geometry pipeline layouts; the consume descriptor set is bound during the geometry passes.

### Geometry `SwGeometry`

* **`WorkPC`** — the vertex+fragment push constants: device addresses for the scene vertex/material/transform/instance/draw-RI/indirect-RC/light buffers, plus light count and IBL parameters.
* **`Resources`** — just holds the push-constant block (the material pipelines themselves are owned by `SwMaterial`).
* **`System`** — issues the indirect draws for the opaque / mask / transparent batches using each material's pipeline.
* **Relations** — consumes the indirect draw lists from `SwCull`, the lights from `SwLighting`, and the IBL maps from `SwIBL`.

### WBOIT `SwWBOIT`

* Weighted Blended Order-Independent Transparency.
* **`Resources`** — the accumulation and revealage images plus the composite pipeline/descriptors.
* **`System`** (`SwSystem::Resizable`) — registers the composite pass that resolves the transparent accumulation onto the draw image.
* **Relations** — composites the output of the transparent geometry pass.

### Pick `SwPick`

* Mouse picking + ImGuizmo instance manipulation.
* **`DrawPC` / `ReadbackPC`** — push constants for drawing instance ids and reading back the id under the cursor; **`ReadbackData`** — the cursor coords + read result.
* **`Resources`** — the id-readback buffer/image and compute pipeline, plus the opaque/transparent and masked id-draw pipelines.
* **`System`** (`SwSystem::Resizable`) — draws ids on demand, reads back the picked instance, and tracks the selected `SwInstance` and current gizmo operation.
* **Relations** — reuses the scene draw lists to render instance ids; the selected instance is manipulated via ImGuizmo and written back to its transform.

### Post-Process `SwPostProcess`

* **`TonemapPC` / `FXAAPC`** — compute push constants (exposure; inverse screen size).
* **`Resources`** — the tonemap and FXAA compute pipelines/descriptors/samplers.
* **`System`** (`SwSystem::Resizable`) — ACES tonemap (always) then optional FXAA, both run in place on the draw image.
* **Relations** — runs after geometry/WBOIT on the resolved HDR draw image; exposes exposure + FXAA toggle to the GUI.

### GUI `SwGui`

* Dear ImGui integration.
* **`SwGuiComponent`** — the panels (Camera, Scene, Options, Stats, Controls).
* **`Resources`** — the ImGui descriptor pool, the component-draw callbacks, and the asset/skybox file browsers.
* **`System`** — builds the dockspace and windows and registers the final GUI pass.
* **Relations** — reads/edits other systems' state (camera, IBL, post-process toggles) and triggers asset/skybox loading on the scene.

---

## Renderer Architecture

* The top-level objects that own the Vulkan device, the windowing/presentation, and the per-frame loop, and that expose shared services to the rest of the engine.

### Renderer `SwRenderer`

* **`SwRenderer`** — the top-level application object. Owns the Vulkan instance / physical device / device, the graphics + compute queues, the VMA allocator, the descriptor allocator, and the major subsystems (`SwLogger`, `SwImmSubmit`, `SwSwapchain`, `SwStats`, `SwEvents`, and the `SwScene`). 
* Runs the main loop `run()` and the per-frame `draw()`. Validation layer strictness is gated on the debug build.
* **`SwVmaAllocator`** — a tiny RAII wrapper that destroys the VMA allocator on teardown.
* **Relations** — constructs and wires up everything, then publishes pointers to its members through the static `SwRenderer::sRendererContext` so the rest of the engine can reach them.

### Swapchain `SwSwapchain`

* **`SwSwapchain`** — owns the SDL window + surface, the `VkSwapchainKHR` and its `SwSwapchainImage`s, the HDR draw image and depth image rendered into, and the frames-in-flight. Triple-buffered (3 swapchain images) with 2 frames of overlap. Handles image acquire / queue submit / present and window resize.
* **`SwFrame`** — per-frame-in-flight state: its own command pool + command buffer, render fence, image-available semaphore, and a per-frame uniform buffer.
* **`SwFrame::Data`** — the per-frame uniform payload uploaded each frame: the camera `SwPerspective`, the `SwSunlight`, and the camera world position.
* **Relations** — owns the draw/depth images the systems render into; `getCurrentFrame()` indexes by frame number; the per-frame buffer's device address is fed into systems' push constants (e.g. `mPerFrameBuffer` in cull/geometry).

### Renderer Context `SwRendererContext`

* **`SwRendererContext`** — a bag of pointers to the shared renderer-wide objects (device, allocator, queues, descriptor allocator, swapchain, immediate submit, events, scene, stats, logger). Reached everywhere via the static `SwRenderer::sRendererContext`, acting as a service locator. Also provides `labelResourceDebug(...)` to name Vulkan objects in debug builds.
* **`VulkanResourceInfo<T>`** — trait specializations (generated by the `DEFINE_VULKAN_RESOURCE_INFO` macros) mapping each `vk` / `vk::raii` handle type to its `vk::ObjectType` and raw handle, used by `labelResourceDebug`.
* **Relations** — included by nearly every `Sw` class; this is how the factories and systems reach the device, allocator, and queues.

### Immediate Submit `SwImmSubmit`

* **`SwImmSubmit`** — runs one-off GPU work outside the frame loop (buffer/image uploads, mipmap generation, IBL bakes) with its own command pool, command buffer, and fence.
* `individualSubmit(...)` records and submits a single callback synchronously; `addCallback(...)` + `queuedSubmit()` batch several callbacks into one submission.

### Logger `SwLogger`

* **`SwLogger`** — Quill-based async logging to console and/or file, and the host of the Vulkan validation-layer debug-messenger callback (`debugMessageFunc`).
* Can suppress specific validation messages or break into the debugger on chosen ones (`SW_DEBUG_BREAK`), and tracks the current frame number for log context.

### Stats `SwStats`

* **`SwStats`** — per-frame timings (frame / draw / scene-update time) and counters (draw calls, initial render items), plus small GPU readback buffers for the cull pass's scratch and published RI counts.
* `perFrameReset()` clears the per-frame counters; surfaced in the GUI stats panel.

---

## Scene Data Model

* Data types from [Data Representation](#data-representation) get flattened into GPU buffers and drive the cull/draw loop. 
* The GPU-Driven rendering approach uploads buffers of relevant data of drawable objects and create indirect draw commands in `SwCull` compute shaders, pointing into those buffers during draw calls.

### Terminology

* An instance (`SwInstance`) is a placement of an entire asset in the world. 
  * If there is a car asset rendered twice, then there are 2 instances of that car.
* A render command (`SwRenderCommand`) is a piece of data passed into the indirect draw buffer during the indirect draw calls. 
  * One is generated for each mesh-node per asset, so the car code have one render command for the wheel mesh-node and one for the body mesh-node, regardless of number of instances.
* A render item (`SwRenderItem`) is the most fundamental piece of data that represents each unit of drawable geometry.
  * For each render command, there is one render item per instance of the asset. 
  * If the car has 2 render commands (wheel and body) and there are 2 instances of the car, then there are 4 render items.

> **One render command per (mesh-node primitive). One render item per (render command × asset instance).**

### Buffer Referencing via Indices

* For a single batch, here an asset with **2 instances** (scene indices 5 and 6) contributes a mesh node with **2 primitives**, producing RC[0] and RC[1], and 2 RIs each:

```
 mRisBuffer (per batch)                 mRcsBuffer (per batch, == indirect draw buffer)
 +--------------------------+           +-------------------------------------------------+
 | RI[0] rc=0  sceneInst=5  |--rc-->    | RC[0] firstRi=0  riCount=(0->cull)              |
 | RI[1] rc=0  sceneInst=6  |--rc-->    |       indexCount firstIndex vertexOffset        |
 | RI[2] rc=1  sceneInst=5  |--rc-->    |       materialIdx nodeXformIdx boundsIdx        |--+
 | RI[3] rc=1  sceneInst=6  |--rc-->    | RC[1] firstRi=2  riCount=(0->cull) ...          |  |
 +--------------------------+           +-------------------------------------------------+  |
        |  mSceneInstanceIndex                       | mBoundsIndex / mNodeTransformIndex /  |
        |                                            | mMaterialIndex (scene-wide buffers)   |
        v                                            v                                       |
 Scene instances buffer                  Scene bounds / node-transform / material buffers <--+
 +-----------------------+
 | ...                   |
 | inst[5]  transform    |   <- shared by RI[0] and RI[2]
 | inst[6]  transform    |   <- shared by RI[1] and RI[3]
 +-----------------------+
```

- **RI → RC**: `RI.mRcIndex` indexes `mRcsBuffer` within the same batch. Every RI knows the draw it belongs to.
- **RI → instance**: `RI.mSceneInstanceIndex` indexes the scene-wide instances buffer for the world transform.
- **RC → scene resources**: `mBoundsIndex`, `mNodeTransformIndex`, `mMaterialIndex`, `mFirstInstance` index the scene-wide bounds, node-transform, material, and instance buffers respectively.
- **RC ↔ RI window**: `RC.mFirstRi` is the base slot in the scene "draw RIs indices" buffer that this RC owns; `RC.mRiCount` is how many of its RIs survived culling (filled in at runtime).

### Culling to Indirect Draw Commands (Render Commands)

* Each cull thread takes one RI and reconstructs its full draw context purely by following the indices above (`SwCullWork.comp.slang`):

```
ri  = mRisBuffer[threadId]
rc  = mRcsBuffer[ri.mRcIndex]
bounds        = mSceneBoundsBuffer[rc.mBoundsIndex]
nodeTransform = mSceneNodeTransformsBuffer[rc.mNodeTransformIndex]
instance      = mSceneInstancesBuffer[ri.mSceneInstanceIndex]
```

* Transforms the bounds AABB by `instance * nodeTransform`, runs the culling tests. 
* If the RI is visible it `InterlockedAdd`s into `rc.mRiCount` to claim a compacted slot and writes the surviving `mSceneInstanceIndex` into `mSceneDrawRisIndicesBuffer[rc.mFirstRi + offset]`.
* At draw time the vertex shader reads back the visible instance.

```
sceneInstanceIndex = mSceneDrawRisIndicesBuffer[in.mInstanceIndex]   // SV_VulkanInstanceID
transform          = mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix
```

---

## Rendering Loop

`SwRenderer::run()` is the main loop: poll SDL events, service any resize (idle the device, then `SwSwapchain::resize()` + `SwScene::resize()`), then run one CPU update followed by one GPU draw, and increment the frame number. Frames overlap 2-deep (see [Swapchain](#swapchain-swswapchain)).

### CPU Update — `SwScene::perFrameUpdate()`

1. Refresh the [GUI](#gui-swgui) and update the [camera](#camera).
2. Apply pending asset/instance loads and unloads. If anything changed, `realign*` the scene-wide offsets, `reloadScene*Buffer` the affected [global buffers](#scene-swscene), and `regenerateRcsAndRis()` to rebuild the [batches](#batch).
3. `refresh()` each system (recomputing push constants and dynamic dependencies), then flush any queued one-shot uploads via [`SwImmSubmit`](#immediate-submit-swimmsubmit).

### GPU Draw — `SwScene::draw()`

1. Wait on the current frame's render fence, reset it, and acquire the next swapchain image.
2. Record a command buffer by assembling the per-frame [render graph](#render-graph-swrendergraph): the passes below are added (some conditionally — skybox, pick, FXAA), the swapchain/draw/depth images are registered as outputs, then `compile()` prunes + topologically sorts them and `execute()` records them with auto-inserted barriers.
3. Transition the swapchain image to present, submit (waiting on the image-available semaphore, signalling the rendered semaphore + render fence), and present.

The per-frame pass order — each pass is owned by the system of the same name (see [Scene and Systems](#scene-and-systems)):

```
ClearImages → [SkyboxDraw] → CullReset → CullEarlyWork → CullEarlyCompact
→ GeometryEarlyOpaque → CullPrepOcclusion → CullLateReset → CullLateWork
→ CullLateCompact → CullPublishCount → GeometryLateOpaque → GeometryMasked
→ GeometryTransparent → [PickDraw → PickReadback → PickWork] → WBOITComposite
→ [LightingBillboard] → Tonemap → [FXAA] → CopyToSwapchain → Gui
```

This realises the two-pass cull (early draw of last frame's visible set, build the HZ pyramid, late occlusion pass) feeding the indirect geometry draws, then transparency composite and post-process, ending in the swapchain copy and GUI overlay. `LightingBillboard` is an optional pass that draws a camera-facing emissive quad for each spawned test light (see the GUI Objects panel) so otherwise-invisible punctual lights are locatable; it only runs when at least one test light exists.

---

## Build

Requirements: CMake 4.3.2+, Ninja, MSVC with C++23, Vulkan 1.4 capable GPU.

```powershell
# Configure
cmake --preset x64-debug    # or x64-release

# Build
cmake --build --preset x64-debug
```

Binaries output to `bin/`.

Shaders are written in [Slang](https://shader-slang.com/) and compiled to SPIR-V with a custom tool:

```powershell
tools\build.bat              # build the shader compiler
.\tools\compileShaders.exe   # compile all .slang -> .spv
```

---

## Dependencies

All dependencies are vendored under `thirdParty/`:

| Library          | Purpose                 |
| ---------------- | ----------------------- |
| Vulkan SDK       | GPU API                 |
| SDL3             | Window and input        |
| glm              | Math                    |
| fastgltf         | glTF 2.0 loading        |
| vk-bootstrap     | Vulkan init boilerplate |
| VMA              | GPU memory allocation   |
| ImGui + ImGuizmo | In-engine UI and gizmos |
| Quill            | Async logging           |
| fmt              | String formatting       |
| stb_image        | Texture loading         |
| magic_enum       | Enum reflection         |

---