# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Build presets are defined in `CMakePresets.json`. Configure and build from the repo root:

```powershell
# Configure (choose a preset)
cmake --preset x64-debug
cmake --preset x64-release

# Build
cmake --build --preset x64-debug
cmake --build --preset x64-release
```

Binaries output to `C:\Projects\SwRenderer\bin`. Generator: Ninja. Compiler: MSVC with C++23.

### Shader Compilation

Shaders are written in Slang (`.slang`) and compiled to SPIR-V (`.spv`) via a custom tool:

```powershell
# Build the shader compiler first
tools\build.bat

# Then run the compiled shader compiler (from repo root)
.\tools\compileShaders.exe
```

Compiled `.spv` files land next to their `.slang` sources in `shaders/`.

## Architecture

This is a **Vulkan 1.4** GPU renderer (the "Sw" prefix is a project name, not "software"). It uses RAII Vulkan C++ bindings (`vulkan_raii.hpp`) and VMA for memory.

### Layer Breakdown

| Directory       | Responsibility                                                                                                |
| --------------- | ------------------------------------------------------------------------------------------------------------- |
| `src/Renderer/` | Core: device, swapchain, context, logger, stats, events, immediate submission                                 |
| `src/Resource/` | Vulkan resource RAII wrappers: buffers, images, pipelines, descriptors, samplers, fences, semaphores, shaders |
| `src/Scene/`    | Render graph, passes, systems, and per-system logic                                                           |
| `src/Data/`     | CPU-side data: assets, meshes, nodes, instances, batches, cameras, materials                                  |
| `src/Gui/`      | Dear ImGui integration and file browser                                                                       |
| `shaders/`      | Slang shaders organised by system (Cull, Geometry, Pick, Skybox, WBOIT, Common)                               |
| `docs/`         | Graphviz diagrams of the render graph and pass dependencies                                                   |
| `thirdParty/`   | Vendored dependencies                                                                                         |

### Key Architectural Patterns

**Render Graph (`SwRenderGraph`, `SwPass`, `SwDependency`)**: Passes declare image/buffer dependencies. The graph topologically sorts passes, prunes unused ones, and resolves Vulkan barriers automatically. Dependencies can be static (same resource every frame) or dynamic (refreshed per frame).

**Resource factories**: `SwBufferFactory<T>` and `SwImageFactory` are template-based factories that track a "generation" counter to detect when a resource has been resized/recreated, so dependent descriptors can be invalidated cheaply.

**Batch/instance pipeline**: Scene geometry is streamed into GPU-resident buffers per material type (opaque, mask, transparent). A compute-shader cull pass (`SwCull`) populates indirect draw commands; `SwGeometry` issues the indirect draws.

**Instances/Render Items/Render Instances**: Instances refer to instances of loaded assets. Render items are generated for every mesh node created from a loaded GLTF file, which hold data for each indirect draw command in VkDrawIndexedIndirectCount. Render instances refer to the instances belonging to a specific render item's mesh node. There are as many render instances of a mesh node as there are instances of a loaded asset from which the mesh node is created from. 

**Systems** (`src/Scene/Sw*.cpp`): Each rendering feature is a self-contained system registered with the scene — `SwCull`, `SwGeometry`, `SwPick`, `SwSkybox`, `SwWBOIT`. They each own their pipelines, descriptors, and pass definitions.

**Frame overlap**: 2 frames in flight; 3-image swapchain (triple-buffered). Per-frame state is indexed via the current frame index.

### Third-party Libraries

- **SDL2** — windowing and events
- **glm** — math (pre-included in PCH)
- **fastgltf** — glTF 2.0 loading
- **vk-bootstrap** — Vulkan instance/device setup
- **VMA** — GPU memory allocation
- **ImGui + ImGuizmo** — in-engine UI and gizmos
- **Quill** — async logging
- **fmt** — string formatting
- **stb_image** — texture loading
- **magic_enum** — enum-to-string reflection

### Code Conventions

- All project classes are prefixed with `Sw` (e.g., `SwRenderer`, `SwBuffer`, `SwScene`).
- Precompiled header (`SwPch.h`) provides STL, GLM, and Vulkan headers — do not re-include them in source files.
- clang-format: Google base, 4-space indent, 160-column limit, left-aligned pointers, attached braces.
- No unit test framework is used; validation is done via Vulkan validation layers (enabled in debug builds).
