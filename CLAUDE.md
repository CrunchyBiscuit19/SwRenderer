# CLAUDE.md

## Build Commands

> **The user builds the C++ themselves — do NOT build or configure the C++ project.** After making
> code changes, hand back to the user to compile; do not run `cmake`/`msbuild`/Ninja yourself. (You
> are still responsible for compiling shaders — see Shader Compilation below.)

> Do not attempt to build the shaders either. The user will run the shader compiler themselves  after shader files are modified.

## Architecture

This is a **Vulkan 1.4** GPU renderer (the "Sw" prefix is a project name, not "software"). It uses RAII Vulkan C++ bindings (`vulkan_raii.hpp`) and VMA for memory.

> **`README.md` is the authoritative, up-to-date architecture reference.** It documents every abstraction
> layer, data type, and system in detail (Vulkan Handle Abstractions, Data Representation, Scene and
> Systems, Renderer Architecture, Scene Data Model, Rendering Loop). Consult it first; the summary below
> is just an orientation. Keep README.md in sync when the architecture changes.

### Layer Breakdown

| Directory       | Responsibility                                                                                                                                       |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/Renderer/` | Core: device, swapchain + per-frame state, renderer context, immediate submission, logger, stats, events                                             |
| `src/Resource/` | Vulkan resource RAII wrappers: buffers, images, samplers, pipelines, descriptors, push constants, shaders, fences, semaphores, command buffers/pools |
| `src/Scene/`    | Scene orchestration (`SwScene`), render graph (`SwRenderGraph`), passes/dependencies (`SwPass`/`SwDependency`), and the `SwSystem` base class        |
| `src/System/`   | Self-contained rendering systems: `SwCull`, `SwGeometry`, `SwIBL`, `SwLighting`, `SwWBOIT`, `SwPick`, `SwPostProcess`, `SwGui`                       |
| `src/Data/`     | CPU-side data: assets, meshes, nodes, instances, batches, cameras, materials, lights                                                                 |
| `shaders/`      | Slang shaders mirroring `src/`: per-data-type modules in `Data/`, per-system directories in `System/`                                                |
| `docs/`         | Graphviz diagrams of the render graph and pass dependencies                                                                                          |
| `thirdParty/`   | Vendored dependencies                                                                                                                                |

### Key Architectural Patterns

**Render Graph (`SwRenderGraph`, `SwPass`, `SwDependency`)**: Passes declare image/buffer dependencies. The graph topologically sorts passes, prunes unused ones, and resolves Vulkan barriers automatically. Dependencies can be static (same resource every frame) or dynamic (refreshed per frame).

**Resource factories**: `SwBufferFactory<T>` and `SwImageFactory` are template-based factories that track a "generation" counter to detect when a resource has been resized/recreated, so dependent descriptors can be invalidated cheaply.

**Batch/instance pipeline**: Scene geometry is streamed into GPU-resident buffers per material type (opaque, mask, transparent). A compute-shader cull pass (`SwCull`) populates indirect draw commands; `SwGeometry` issues the indirect draws. The instance / render-command / render-item flattening is documented in README.md's *Scene Data Model*.

**Systems** (`src/System/`): Each rendering feature is a self-contained system in its own `SwX` namespace, registered with the scene and owning its pipelines, descriptors, push constants, and pass definitions — `SwCull`, `SwGeometry`, `SwIBL` (which now owns the skybox draw), `SwLighting`, `SwWBOIT`, `SwPick`, `SwPostProcess`, `SwGui`. They derive from `SwSystem` (and `SwSystem::Resizable` if size-dependent).

**Frame loop**: `SwRenderer::run()` drives a CPU update (`SwScene::perFrameUpdate()`) then a GPU draw (`SwScene::draw()`) per frame. 2 frames in flight; 3-image swapchain (triple-buffered). Per-frame state is indexed via the current frame index. See README.md's *Rendering Loop* for the full per-frame pass order.

### Third-party Libraries

- **SDL3** — windowing and events
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
- **Slang struct layout is relaxed — do NOT hand-pad for std140/std430.**
- clang-format: Google base, 4-space indent, 160-column limit, left-aligned pointers, attached braces.
- No unit test framework is used; validation is done via Vulkan validation layers (enabled in debug builds).
- Avoid leaving excessive comments.
- If comments must be written, every line must contain one full sentence. Do not break up sentences into multi-line comments.
- No emdashes or semicolons are to be used in comments or documentation. 
- Documentation with LaTeX must follow a few rules:
  - Inline formulas only (the single `$`, not `$$`)
  - Use `\displaystyle` before summation sigma, the integral sign, etc.
  - Use `\dfrac` for fractions