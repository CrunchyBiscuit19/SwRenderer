# SwRenderer

A Vulkan 1.4 renderer written in C++23, targeting GPU-driven rendering with a render-graph architecture. Built primarily as a learning project and testbed for modern rendering techniques.

---

## Features

### Rendering Pipeline

- GPU-driven indirect drawing — geometry is batched by material type; a compute cull pass populates indirect draw command buffers
- Render graph with automatic Vulkan barrier insertion and topological pass ordering
- Weighted Blended Order-Independent Transparency (WBOIT) for transparent geometry
- Frustum culling (6-plane AABB) in a compute shader
- Depth pyramid construction (occlusion culling infrastructure present but disabled)
- Cubemap skybox
- Color-based mouse picking (object + instance identification)
- Dear ImGui editor with ImGuizmo

### Asset Support

- glTF 2.0 scene loading via fastgltf
- PBR material data (albedo, normal, metallic-roughness, AO, emissive) loaded from glTF; partial shader use — see checklist below
- Runtime asset loading via in-engine file browser

---

## PBR Implementation Checklist

### Material Inputs

- [x] Albedo / base color texture
- [ ] Normal mapping (texture loaded, tangent-space transform not applied in shader)
- [ ] Metallic-roughness workflow — texture and factors loaded, no BRDF evaluation yet
- [ ] Ambient occlusion map (texture loaded, not sampled in shader)
- [ ] Emissive map (texture loaded, not sampled in shader)

### Lighting Model

- [x] Directional light (`SwSunlight` — direction, intensity, color)
- [ ] PBR BRDF (Cook-Torrance GGX) — current lighting is Lambertian diffuse only
- [ ] Point lights
- [ ] Spot lights
- [ ] Shadow mapping

### Image-Based Lighting

- [ ] Diffuse irradiance map / spherical harmonics
- [ ] Specular pre-filtered environment map
- [ ] BRDF LUT (split-sum approximation)
- [ ] IBL contribution in lighting shader

### Post-Processing

- [ ] HDR render target + tone mapping (currently LDR output)
- [ ] Bloom
- [ ] SSAO
- [ ] Anti-aliasing (MSAA / TAA / FXAA)

### Rendering Passes

- [x] Opaque geometry pass (GPU indirect draw, batched by material)
- [x] Alpha-masked geometry (alpha discard in fragment shader)
- [x] Transparency pass (WBOIT — accumulation + reveal + composite)
- [x] Skybox pass
- [x] Mouse pick pass (draw + compute readback)
- [ ] Depth pre-pass
- [x] Frustum culling (compute, 6-plane AABB)
- [ ] Occlusion culling (depth pyramid built; culling logic present but commented out)

---

## Roadmap & Future Work

Planned lighting/shading features, ordered roughly by a sensible build sequence (later items
depend on earlier ones). Difficulty is rated relative to *this* renderer — the render graph
auto-inserts barriers and the GPU-driven cull/indirect path already exists, which makes adding
passes cheap but makes anything touching the per-cascade/per-light draw path more involved.
Several items overlap the **PBR Implementation Checklist** above; see the detailed plan in
[`docs/lighting-plan.md`](docs/lighting-plan.md) for Phases 1–3.

Legend — **Easy**: a shader + a buffer. **Medium**: a new pass or non-trivial data plumbing.
**Hard**: a whole subsystem.

| # | Feature | Difficulty | Feasibility notes |
|---|---------|------------|-------------------|
| 1 | Basic lighting (directional Lambert/Phong) | Easy | Mostly already present; just un-hardcode the sun into `PerFrameData`. Phase 1. |
| 2 | Gamma correction | Easy | Largely handled by sRGB swapchain/texture formats already; make the convention explicit and audit linear-space math. Do alongside HDR. |
| 3 | Lighting maps (diffuse/spec/normal driving shading) | Easy | All 5 PBR textures already loaded; just sample and use them. |
| 4 | Light casters (attenuation/cone math) | Easy | Pure shader math once light data is in a buffer. |
| 5 | Multiple lights | Easy–Medium | Needs a `Light` SSBO + count; trivial in forward until counts grow (then → Forward+). Phase 3. |
| 6 | Point / Directional / Spot lights | Easy | One tagged `Light` struct covers all three (`KHR_lights_punctual`). Tie to scene via `SwLightNode`. Phase 3. |
| 7 | Normal mapping | Medium | **Blocker:** `Sw::Vertex` has no tangents — must add the attribute and generate them (MikkTSpace) on load. TBN in shader is the easy part. |
| 8 | PBR theory + PBR lighting (Cook-Torrance GGX) | Medium | The core direct-lighting BRDF. Straightforward once lighting maps + lights exist. Phase 2. |
| 9 | HDR render target + tone mapping | Medium | Switch geometry target to a float format, add a tone-map pass. Render graph makes the pass trivial; the work is threading the new target through WBOIT composite/FXAA. |
| 10 | Parallax / parallax-occlusion mapping | Medium | Easy once tangents (item 7) exist, **but** glTF rarely ships height maps (`KHR_materials_*` displacement is uncommon) — asset support is the real snag. |
| 11 | Physically-based bloom | Medium | Mip-chain down/up-sample (CoD/Jimenez). Needs HDR first. You already run compute post passes (FXAA), so the pattern is established. |
| 12 | SSAO | Medium–Hard | Needs view-space depth + normals. You're forward-only, so this forces a depth/normal prepass (already a TODO) before the AO + blur passes. |
| 13 | PBR IBL (irradiance + prefilter + BRDF LUT) | Hard | Several one-time precompute passes (split-sum). Builds on the existing cubemap skybox infra, but the prefilter mip chain + BRDF LUT is a real chunk of work. |
| 14 | Cascaded shadow maps | Hard | The biggest single subsystem: per-cascade depth render, light-space matrices, cascade splitting, PCF/filtering — and ideally per-cascade culling through the existing GPU-driven indirect path. |
| 15 | Area lights | Hard | Real-time area lights (LTC) are mathematically heavy and a notable jump beyond punctual lights; defer until punctual + IBL are solid. |
| 16 | Forward+ (tiled/clustered light culling) | Hard | Compute light binning + depth prepass + per-tile light lists. Only pays off at high light counts; your compute-cull background helps conceptually but it's a substantial system. |

**Overall feasibility:** items 1–6 are quick wins that mostly reuse existing data and scaffolding.
Item 7 (tangents) is the first real data-model change and gates normal/parallax mapping. Items
8–9 complete a respectable direct-lit PBR pipeline. Everything from 12 onward is "new subsystem"
territory — individually feasible in this architecture, but each is a project in its own right.
The two genuinely hard, high-value milestones are **cascaded shadow maps** and **PBR IBL**;
**area lights** and **Forward+** are the most speculative and should come last.

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

### Shaders

Shaders are written in [Slang](https://shader-slang.com/) and compiled to SPIR-V with a custom tool:

```powershell
tools\build.bat              # build the shader compiler
.\tools\compileShaders.exe   # compile all .slang -> .spv
```

---

## Dependencies

All dependencies are vendored under `thirdParty/`:

| Library | Purpose |
|---------|---------|
| Vulkan SDK | GPU API |
| SDL2 | Window and input |
| glm | Math |
| fastgltf | glTF 2.0 loading |
| vk-bootstrap | Vulkan init boilerplate |
| VMA | GPU memory allocation |
| ImGui + ImGuizmo | In-engine UI and gizmos |
| Quill | Async logging |
| fmt | String formatting |
| stb_image | Texture loading |
| magic_enum | Enum reflection |

---

## Project Structure

```
src/
  Renderer/   - device, swapchain, context, events, logging
  Resource/   - Vulkan resource wrappers (buffer, image, pipeline, descriptor, ...)
  Scene/      - render graph, passes, and per-system logic (cull, geometry, WBOIT, pick, skybox)
  Data/       - CPU-side scene data (asset, mesh, node, instance, material, camera)
  Gui/        - ImGui integration
shaders/      - Slang shaders, one subdirectory per system
docs/         - Graphviz render graph and pass dependency diagrams
tools/        - Shader compiler source and build script
resources/    - Test assets (New Sponza glTF, skybox faces)
```

---

## Scene Data Model

The renderer is GPU-driven: the CPU uploads a flat description of *what could be drawn*, and a compute cull pass decides *what actually gets drawn* each frame. Understanding the pipeline means understanding five CPU-side concepts and how they are flattened into GPU buffers. The headline relationship is:

> **One render command per (mesh-node primitive). One render item per (render command × asset instance).**

### The five concepts

| Concept | Type | Lives in | One per... |
|---------|------|----------|------------|
| **Asset** | `SwAsset` | — | loaded glTF file |
| **Node** / **Mesh Node** | `SwNode` / `SwMeshNode` | `SwAsset::mNodes` | glTF node (mesh nodes additionally reference a `SwMesh`) |
| **Instance** | `SwInstance` | `SwAsset::mInstances` | placed copy of the whole asset (its own transform) |
| **Render Command (RC)** | `SwRenderCommand` | `SwBatch::mRcs` | primitive of a mesh node |
| **Render Item (RI)** | `SwRenderItem` | `SwBatch::mRis` | RC × instance |

**Asset** (`SwAsset`) — everything parsed from one glTF file: its meshes, materials, node hierarchy, per-mesh bounds (AABBs), and a list of **instances**. When an asset is added to the scene it is given contiguous base offsets into the scene-wide GPU buffers: `mFirstNodeTransformInScene`, `mFirstMaterialInScene`, `mFirstInstanceInScene`, and `mFirstBoundInScene`. These offsets are what turn an asset's *relative* indices into *scene-absolute* indices.

**Nodes and Mesh Nodes** (`SwNode` / `SwMeshNode`) — the glTF transform hierarchy. A plain `SwNode` is just a named transform with children; an `SwMeshNode` additionally holds a reference to a `SwMesh`. A `SwMesh` is a list of `SwPrimitive`s, where each primitive is an index range (`mRelativeFirstIndex`, `mIndexCount`, `mRelativeVertexOffset`) plus the `SwMaterial` it is drawn with. The primitive is the smallest drawable unit, because a single mesh can mix materials (and therefore pipelines).

**Instances** (`SwInstance`) — a placement of the *entire* asset in the world, carrying its own transform matrix (`SwInstance::Data::mTransformMatrix`). Calling `SwAsset::createInstance(...)` appends one. If an asset has 3 instances, the whole node hierarchy is effectively drawn 3 times, each with a different instance transform composed on top of each node's local transform.

**Render Commands** (`SwRenderCommand`) — produced by `SwMeshNode::generateRcsAndRis()`, which walks the node tree and emits **one RC per primitive of each mesh node**. The first five fields are laid out to match `VkDrawIndexedIndirectCommand`:

| Field | Role |
|-------|------|
| `mIndexCount` | index count of the primitive |
| `mRiCount` | **instance count — starts at 0**, incremented by the cull shader as RIs survive |
| `mFirstIndex` | `mesh.mFirstIndexInScene + primitive.mRelativeFirstIndex` |
| `mVertexOffset` | `mesh.mVertexOffsetInScene + primitive.mRelativeVertexOffset` |
| `mFirstRi` | base offset of this RC's render items (doubles as `firstInstance`) |
| `mMaterialIndex` | `asset.mFirstMaterialInScene + material.mRelativeMaterialIndex` |
| `mNodeTransformIndex` | `asset.mFirstNodeTransformInScene + node.mRelativeNodeIndex` |
| `mAssetIndex` / `mModelIndex` | owning asset id |
| `mFirstInstance` | `asset.mFirstInstanceInScene` (base into the scene instances buffer) |
| `mBoundsIndex` | `asset.mFirstBoundInScene + mesh.mRelativeFirstBounds` |

So an RC bundles a draw range with *indices into the scene-wide material, transform, bounds, and instance buffers* — everything the GPU needs except which specific instances are visible.

**Render Items** (`SwRenderItem`) — the (RC × instance) cross product. For each RC, `generateRcsAndRis()` emits **one RI per instance of the asset**. Each RI is tiny:

- `mRcIndex` — which RC this item belongs to.
- `mSceneInstanceIndex` — `asset.mFirstInstanceInScene + i`, i.e. the absolute index of instance *i* in the scene instances buffer.

The RI is the unit of culling: the cull shader dispatches **one thread per RI**.

### Batches

RCs and RIs are not global lists — they are grouped into `SwBatch`es. Each batch holds all RCs/RIs that share one **graphics pipeline** (`pipelineId`). Batches are stored in per-material-type maps (opaque / mask / transparent), keyed by pipeline id, on the scene. `SwBatch::sFirstRiOffset` is a running counter that hands each RC a unique, non-overlapping `mFirstRi` window across all batches, so the scene-wide "draw RIs indices" buffer never has two RCs writing to the same slots.

### How it links together in buffers

For a single batch, the relationship between the RIs buffer, the RCs buffer, and the scene-wide instances buffer looks like this. Here an asset with **2 instances** (scene indices 5 and 6) contributes a mesh node with **2 primitives**, producing RC[0] and RC[1], and 2 RIs each:

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

Reading the links:

- **RI → RC**: `RI.mRcIndex` indexes `mRcsBuffer` within the same batch. Every RI knows the draw it belongs to.
- **RI → instance**: `RI.mSceneInstanceIndex` indexes the scene-wide instances buffer for the world transform.
- **RC → scene resources**: `mBoundsIndex`, `mNodeTransformIndex`, `mMaterialIndex`, `mFirstInstance` index the scene-wide bounds, node-transform, material, and instance buffers respectively.
- **RC ↔ RI window**: `RC.mFirstRi` is the base slot in the scene "draw RIs indices" buffer that this RC owns; `RC.mRiCount` is how many of its RIs survived culling (filled in at runtime).

### What the cull pass does with this layout

Each cull thread takes one RI and reconstructs its full draw context purely by following the indices above (`SwCullWork.comp.slang`):

```
ri  = mRisBuffer[threadId]
rc  = mRcsBuffer[ri.mRcIndex]
bounds        = mSceneBoundsBuffer[rc.mBoundsIndex]
nodeTransform = mSceneNodeTransformsBuffer[rc.mNodeTransformIndex]
instance      = mSceneInstancesBuffer[ri.mSceneInstanceIndex]
```

It transforms the bounds AABB by `instance * nodeTransform`, runs the frustum (and, in the late phase, occlusion) test, and if the RI is visible it `InterlockedAdd`s into `rc.mRiCount` to claim a compacted slot and writes the surviving `mSceneInstanceIndex` into `mSceneDrawRisIndicesBuffer[rc.mFirstRi + offset]`.

Because the RCs are laid out as `VkDrawIndexedIndirectCommand`s, the populated `mRiCount` *is* the instance count and `mFirstRi` *is* the `firstInstance`. At draw time the vertex shader reads back the visible instance:

```
sceneInstanceIndex = mSceneDrawRisIndicesBuffer[gl_InstanceIndex]   // gl_InstanceIndex starts at mFirstRi
transform          = mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix
```

— closing the loop from RI, through the compacted draw list, back to the per-instance transform.
