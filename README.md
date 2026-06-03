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
