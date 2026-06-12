# Lighting Implementation Plan

Incremental path from the current hardcoded sun to a data-driven PBR pipeline with multiple
punctual lights. Each phase produces correct, shippable pixels before the next begins.

---

## Motivation

Lighting today is hardcoded in the fragment shaders. `SwGeometryWorkOpaqueMasked.frag.slang`
and `SwGeometryWorkTransparent.frag.slang` both do a flat Lambert diffuse against
`sunlightDir = (1,0,0)` with constant ambient (`// TODO un-hardcode sunlight data`). Material
PBR data is fully loaded but unused — `MaterialConstant` carries metallic/roughness, emissive,
normal scale and occlusion strength, and 5 textures are bound per material, yet only the base
color texture is sampled.

The work splits into three phases ordered by dependency, not importance. The per-pixel shading
equation is the hard part and is identical for 1 light or 1000, so it is proven with a single
directional light first. "More lights" is then a generalization, not a rewrite.

---

## Phase Sequence

```
Phase 1  Data-driven sun + emissive + world-space normals   (one PR)
Phase 2  PBR material shading (metallic-roughness BRDF)
Phase 3  Multiple punctual lights (Light SSBO + SwLightNode)
Phase 4  Image-based lighting (diffuse irradiance + specular prefilter + BRDF LUT)
```

---

## Phase 1 — Data-driven directional light + emissive

Goal: remove the hardcoded sun, drive it from the GUI, fix the normal transform, and add the
emissive term. Smallest end-to-end slice that exercises the full data path.

### 1a. World-space normals (prerequisite — do not skip)

`SwGeometry.vert.slang:30` currently passes `out.mNormal = v.mNormal`, i.e. **model-space**
normals, while the light direction is world-space. Lighting is silently wrong the moment any
instance or node rotates.

- Compute the model matrix already assembled in the VS (`instanceTransform * nodeTransform`).
- Output a world-space normal: `normalize(mul((float3x3)transpose(inverse(model)), v.mNormal))`.
- Also output **world-space position** (`mul(model, position).xyz`) — unused for a directional
  light, but Phase 2 (specular) and Phase 3 (point/spot attenuation) require it, so add it now.
- Extend `SwGeometry::VSOutput`/`FSInput` (`SwGeometry.h.slang`) with `mWorldPos` and keep
  `mNormal` as the world-space normal.

### 1b. Sunlight in PerFrameData

`PerFrameData` (`SwCommon.h.slang:14`) currently holds only `Perspective`. A `Sw::Sunlight`
struct is **already declared** (`SwCommon.h.slang:83`) but unused.

- Add `Sunlight mSunlight;` to `PerFrameData` in `SwCommon.h.slang`.
- Mirror the layout in the C++ struct uploaded by `SwFrame::update()`
  (`SwSwapchain.cpp:21`). Today that copies a bare `SwPerspective`; introduce a
  `SwPerFrameData { SwPerspective mPerspective; SwSunlight mSunlight; }` and copy the whole
  thing. Watch std140/scalar alignment — match the Slang layout exactly.
- Add an `SwSunlight` POD on the C++ side (ambient, direction, intensity, color) owned by the
  scene/camera so it survives across frames and the GUI can edit it.

### 1c. Revive the GUI panel

A commented-out sunlight panel already exists at `SwGui.cpp:116` (it references a stale
`mScene.mPerspective.mData.*` path). Repoint it at the new `SwSunlight` owner and uncomment:
ambient color, sunlight color, direction, intensity.

### 1d. Consume in the fragment shaders

In both `SwGeometryWorkOpaqueMasked.frag.slang` and `SwGeometryWorkTransparent.frag.slang`:

- Replace the hardcoded `sunlightDir` / `sunlightPower` with values read from
  `workPc.mPerFrameBuffer->mSunlight` (the frag shaders will need the per-frame buffer address —
  it already reaches the VS via `WorkPC`; thread it to the FS or read via push constant).
- Diffuse term: `max(dot(N, -L), 0) * sunColor * intensity` (decide and document the direction
  convention — recommend storing the direction the light travels, shade with `-L`).
- **Emissive**: sample emissive texture (index `mMaterialIndex * NUM_PBR_IMAGES + 4`) times
  `mEmissiveFactor` and add to the final color. This is *not* a light — it makes the surface
  glow but does not illuminate neighbours. It is trivial and orthogonal, so it rides along here.

### Phase 1 done when

The sun is editable in the GUI, a rotating mesh is shaded correctly (proves world-space
normals), and emissive materials visibly glow.

---

## Phase 2 — PBR material shading

Goal: replace the Lambert term with a metallic-roughness Cook-Torrance BRDF, consuming the
material textures already loaded but currently ignored.

### 2a. Tangents (prerequisite for normal mapping)

`Sw::Vertex` (`SwCommon.h.slang:18`) has **no tangent** — grep confirms tangents are absent
across the codebase. Normal mapping needs a tangent basis.

- Add `float4 mTangent` to `Sw::Vertex` and the matching C++ vertex layout.
- Load tangents from glTF in `SwAsset.cpp`; generate them (e.g. per-triangle, or MikkTSpace)
  when the asset lacks a tangent attribute.
- Build the TBN in the VS and transform the sampled normal map (index `+2`) by `mNormalScale`.

### 2b. BRDF

In a shared shading function (consider a `SwBRDF` Slang module imported by both frag shaders):

- Sample metallic-roughness (index `+1`) × `mMetallicRoughnessFactor`; occlusion (index `+3`)
  × `mOcclusionStrength`.
- Implement Cook-Torrance: GGX normal distribution, Smith geometry, Fresnel-Schlick.
- Combine diffuse (albedo, non-metal) + specular, multiply by the sun term from Phase 1.
- Apply ambient occlusion to the ambient/IBL term only.
- Keep a simple constant or hemisphere ambient for now; full IBL is out of scope.

### 2c. Tone mapping

Shading now produces HDR values. Add a tone-map + (if not already sRGB-handled by the
swapchain format) gamma step at the end of the geometry frag output. Verify against the
existing `SRGB_IMAGE_FORMAT` / `UNORM_IMAGE_FORMAT` choices in `SwMaterial.h`.

### Phase 2 done when

Metallic and dielectric materials read correctly, normal maps perturb shading, and a roughness
sweep behaves physically — all under the single Phase 1 sun.

---

## Phase 3 — Multiple punctual lights

Goal: generalize the proven single-light equation to an array of directional / point / spot
lights, sourced from the scene graph.

### 3a. Light GPU representation

One tagged struct in `SwCommon.h.slang`, mirrored in C++, stored in an SSBO addressed by device
address (same pattern as the other scene buffers):

```slang
struct Light {
    float3 position;   // world space; ignored for directional
    uint   type;       // 0=dir 1=point 2=spot
    float3 direction;  // world space; ignored for point
    float  range;      // <=0 = infinite
    float3 color;      // linear RGB
    float  intensity;  // glTF units: lux (dir) / candela (point,spot)
    float  innerCos;   // spot inner cone, cos
    float  outerCos;   // spot outer cone, cos
    float2 _pad;
};
```

Add a count + buffer to `PerFrameData` or `SwScene`. The Phase 1 sun becomes `type == 0`,
either as element 0 or kept separate (directional lights have no attenuation and usually own the
main shadow map).

### 3b. SwLightNode

Add `SwLightNode : SwNode` alongside `SwMeshNode` (`SwNode.h:47`). glTF attaches lights to
nodes via `KHR_lights_punctual`; the node's world transform supplies position (translation) and
direction (−Z axis).

- During the hierarchy walk (`refreshTransform` / a `generateRcsAndRis`-style pass), a light
  node extracts position/direction from `mWorldTransform` and writes its `Light` entry — exactly
  analogous to how `SwMeshNode::generateRcsAndRis()` emits draw data.
- Parse `KHR_lights_punctual` in `SwAsset.cpp` (fastgltf supports it) and instantiate
  `SwLightNode`s when building the node hierarchy.
- Because the light is a node, parenting it under a mesh node makes the light follow the mesh.

### 3c. Shading loop

Replace the single sun call with `for (i < lightCount) accumulate(light[i])`, applying
attenuation for point (`1/d²`, clamped by `range`) and the cone falloff for spot
(`smoothstep(outerCos, innerCos, cosAngle)`).

### Out of scope (note for later)

- **Shadows** — per-light shadow maps / atlas; the biggest follow-on system.
- **Light culling** — clustered/forward+ once light counts grow.
- **Emissive as real light** — emissive meshes illuminating their surroundings needs GI
  (ray tracing, probes, VPLs). Phase 1 emissive only makes surfaces glow, it does not cast light.

---

## Phase 4 — Image-based lighting

Goal: replace the constant/hemisphere ambient term left as a placeholder in Phase 2b with a real
environment-derived ambient — diffuse irradiance and specular reflections from the skybox. This is
the term that makes metals reflect their surroundings and dielectrics pick up sky/ground color.

Depends on Phase 2 (a Cook-Torrance BRDF with a cleanly separated ambient/IBL term) and Phase 2c
(tone mapping — IBL outputs HDR irradiance). Independent of Phase 3: IBL and punctual lights add
into the same accumulator, so either can land first once Phase 2 is done.

### 4a. Environment source

`SwSkybox` already loads an environment cubemap; reuse it as the IBL source rather than introducing
a second one.

- Require an HDR/float environment cubemap (the prefilter integrals are meaningless on an LDR,
  already-tone-mapped image). Confirm the skybox cubemap format is float (e.g. `R16G16B16A16_SFLOAT`),
  and load equirectangular `.hdr` → cubemap if the current path is LDR only.
- These maps are static per environment: bake once when the skybox changes, not per frame. A
  one-shot compute/render pass driven off a "skybox dirty" flag, analogous to how other one-time
  resources are built.

### 4b. Diffuse irradiance map

- Convolve the environment cubemap into a small (e.g. 32×32 per face) irradiance cubemap: for each
  output direction, integrate cosine-weighted radiance over the hemisphere.
- A compute shader (new `shaders/IBL/` system) writing a cubemap `SwImage`, owned by the IBL system
  and exposed to the geometry frag shaders via descriptor like the other bound textures.

### 4c. Specular prefilter + BRDF LUT

- **Prefiltered environment map**: a mip-chained cubemap where mip level encodes roughness; each
  level is the GGX-importance-sampled convolution of the environment for that roughness. Sample with
  `roughness * maxMip` at runtime (split-sum approximation).
- **BRDF integration LUT**: a 2D `R16G16_SFLOAT` lookup over (NdotV, roughness) holding the scale/bias
  for the environment Fresnel term. Bake once at startup — it is independent of the environment, so it
  never needs rebuilding.

### 4d. Consume in the shading function

In the shared `SwBRDF` module from Phase 2b, replace the placeholder ambient with:

- Diffuse: `irradiance(N) * albedo * (1 - metallic)`.
- Specular: `prefiltered(R, roughness) * (F0 * brdfLut.x + brdfLut.y)`.
- Multiply the whole IBL contribution by the material occlusion term (Phase 2b already routes AO to
  the ambient/IBL slot), and add it to the direct-light accumulator from Phases 1/3.

### Phase 4 done when

A rough dielectric picks up sky/ground color, a smooth metal mirrors the environment, and a
roughness sweep transitions smoothly from sharp reflection to diffuse — with the punctual lights from
Phases 1/3 still adding on top.

### Out of scope (note for later)

- **Local reflection probes** — multiple parallax-corrected IBL volumes for indoor scenes; Phase 4 is
  a single global environment only.
- **Screen-space reflections** — dynamic-geometry reflections that IBL (static environment) cannot
  capture.
- **Dynamic environment capture** — re-baking the probe from the live scene rather than a fixed
  skybox.

---