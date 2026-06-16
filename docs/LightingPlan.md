# Lighting Implementation Plan

The incremental path from a hardcoded sun to a data-driven PBR pipeline is **complete**:

- **Phase 1 — Data-driven directional light + emissive**: world-space normals/position, `Sunlight`
  in `PerFrameData`, GUI panel, and emissive term — done.
- **Phase 2 — PBR material shading**: tangents + normal mapping, Cook-Torrance metallic-roughness
  BRDF (`SwBRDF` module), and tone mapping (`SwTonemap.comp.slang`) — done.
- **Phase 3 — Multiple punctual lights**: `Light` SSBO, `SwLight`/`SwLightNode`,
  `KHR_lights_punctual` parsing, and directional/point/spot accumulation — done.
- **Phase 4 — Image-based lighting**: diffuse irradiance, specular prefilter, BRDF LUT (the
  `SwIBL` system + `shaders/IBL/`), consumed via the split-sum ambient term — done.

What remains is the follow-on work deferred during the phases above.

---

## Remaining work

### Shadows

Per-light shadow maps / atlas. The biggest follow-on system. Directional lights usually own the
main shadow map; point/spot need cube / perspective shadow maps respectively.

### Light culling

Clustered / forward+ light assignment, needed once light counts grow beyond a naive per-fragment
loop over every light in the scene buffer.

### Emissive as real light

Emissive surfaces currently glow but do not illuminate neighbours. Making emissive meshes cast
light needs global illumination (ray tracing, probes, or VPLs).

### Local reflection probes

Multiple parallax-corrected IBL volumes for indoor scenes. The current IBL is a single global
environment only.

### Screen-space reflections

Dynamic-geometry reflections that a static-environment IBL cannot capture.

### Dynamic environment capture

Re-baking the IBL probe from the live scene rather than a fixed skybox.
