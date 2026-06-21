# Shadow Mapping — Vulkan + Slang

Port of [LearnOpenGL: Shadow Mapping](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping) to Vulkan-hpp (RAII) and Slang shaders.

---

## Overview

Shadow mapping is a two-pass technique:

1. **Shadow pass** — render the scene from the light's point of view, writing only depth into a *shadow map* texture.
2. **Lighting pass** — render the scene normally; for each fragment, project it into light space and compare its depth against the shadow map to determine if it is occluded.

---

## OpenGL → Vulkan translation notes

These are the non-obvious differences that require code changes beyond a mechanical syntax swap.

### Depth range (the most important difference)

OpenGL NDC Z is `[-1, 1]`; Vulkan's is `[0, 1]`.

LearnOpenGL remaps all three components with `projCoords * 0.5 + 0.5`. In Vulkan **only XY need the remap** — Z comes out of the projection already in `[0, 1]`.

Also set `#define GLM_FORCE_DEPTH_ZERO_TO_ONE` project-wide (before any GLM include) so that `glm::ortho` and `glm::perspective` produce correct Vulkan projection matrices. GLM also flips the Y axis in Vulkan clip space, which `GLM_FORCE_DEPTH_ZERO_TO_ONE` handles automatically.

### Framebuffer → Render passes

`glDrawBuffer(GL_NONE)` / `glReadBuffer(GL_NONE)` (depth-only FBO) maps to a Vulkan render pass with **no colour attachment at all** — just a single `VkAttachmentDescription` for the depth image. The shadow pipeline needs no colour blend state.

### Peter-panning fix

OpenGL: `glCullFace(GL_FRONT)`.
Vulkan: set `cullMode = vk::CullModeFlagBits::eFront` on the shadow pipeline's `VkPipelineRasterizationStateCreateInfo`, with `frontFace` matching the rest of your geometry.

### Over-sampling fix (border clamping)

OpenGL: `GL_CLAMP_TO_BORDER` + white border colour.
Vulkan: `addressModeU/V/W = vk::SamplerAddressMode::eClampToBorder` and `borderColor = vk::BorderColor::eFloatOpaqueWhite`. The sampler returns `1.0` for depth samples outside the `[0,1]` UV range, so fragments outside the light frustum are never considered in shadow.

### Explicit synchronisation

There is no equivalent in OpenGL — the driver handles it. In Vulkan you need an explicit `vkCmdPipelineBarrier` between the two passes, transitioning the shadow map image from `DEPTH_STENCIL_ATTACHMENT_WRITE` → `SHADER_READ`. See the render loop below.

### Descriptor sets vs. uniforms

OpenGL's `glUniform*` / `glBindTexture` calls map to a single descriptor set (set 0) with three bindings: the scene UBO, the diffuse texture, and the shadow map. Per-object data (the model matrix) is passed via push constants to avoid allocating a new descriptor set per object.

---

## Pass 1 — Shadow pass (`shadow_pass.slang`)

Renders the scene from the light's POV. Only position is needed — normals and UVs are ignored. The empty fragment shader is intentional: the hardware writes depth automatically.

The **peter-panning fix** is applied at the pipeline level (front-face culling), not in the shader. This writes back-face depth, which is always ≥ front-face depth and avoids the self-shadowing bias problem entirely for solid geometry.

```slang
// shadow_pass.slang
// Pass 1: Render scene from the light's POV to build the depth (shadow) map.
// Compile with: slangc shadow_pass.slang -profile glsl_450 -o shadow_pass.spv
//   (or -profile sm_6_0 for DXIL)

// ── Push constants ──────────────────────────────────────────────────────────
// Keep the layout identical between the two passes so you can share a
// vk::PipelineLayout (or at least a compatible one).
struct ShadowPushConstants
{
    float4x4 lightSpaceMatrix;  // lightProjection * lightView
    float4x4 model;
};

[[vk::push_constant]]
ShadowPushConstants pc;

// ── Vertex shader ────────────────────────────────────────────────────────────
struct VSIn
{
    [[vk::location(0)]] float3 position : POSITION;
    // normals / uvs not needed in the depth pass — just ignore them
};

struct VSOut
{
    float4 clipPos : SV_Position;
};

[shader("vertex")]
VSOut vertMain(VSIn vsIn)
{
    VSOut vsOut;
    vsOut.clipPos = mul(pc.lightSpaceMatrix, mul(pc.model, float4(vsIn.position, 1.0)));
    return vsOut;
}

// ── Fragment shader ──────────────────────────────────────────────────────────
// No colour output — the depth attachment is written automatically by the
// rasteriser.  An empty entry point is enough.
//
// Peter-panning fix: enable front-face culling on the shadow-pass pipeline
// (vk::RasterizationStateCreateInfo::cullMode = vk::CullModeFlagBits::eFront).
// That way we write the *back-face* depth, which is always ≥ the front-face
// depth and neatly side-steps the self-shadowing offset problem.

[shader("fragment")]
void fragMain()
{
    // Nothing to do — hardware writes gl_FragCoord.z into the depth attachment.
}
```

---

## Pass 2 — Lighting pass (`lighting_pass.slang`)

Blinn-Phong lighting with three improvements over the LearnOpenGL baseline baked in:

1. **Slope-scaled bias** — bias scales with the angle between the surface normal and light direction, reducing acne on oblique surfaces without over-biasing flat ones.
2. **Border-clamp over-sampling fix** — handled by the sampler (see `makeShadowSampler` below), plus an early-out for `z > 1.0` to handle the far-plane edge case.
3. **3×3 PCF kernel** — samples 9 surrounding texels and averages the shadow results, producing soft edges.

### Descriptor set layout

| Set | Binding | Type | Stage | Contents |
|-----|---------|------|-------|----------|
| 0 | 0 | Uniform buffer | Vert + Frag | `SceneUBO` |
| 0 | 1 | Combined image sampler | Frag | Diffuse texture |
| 0 | 2 | Combined image sampler | Frag | Shadow map |

Push constants carry the per-object model matrix (vertex stage only).

### Shadow map image requirements

```
format  = vk::Format::eD32Sfloat   // or eD16Unorm for tighter packing
usage   = eDepthStencilAttachment | eSampled
```

### Shadow map sampler requirements

```
magFilter / minFilter = eNearest
addressModeU/V/W      = eClampToBorder
borderColor           = eFloatOpaqueWhite   // depth = 1.0 outside frustum
compareEnable         = VK_FALSE            // manual comparison in shader
```

```slang
// lighting_pass.slang
// Pass 2: Render the scene with Blinn-Phong lighting and PCF shadow mapping.
// Compile with: slangc lighting_pass.slang -profile glsl_450 -o lighting_pass.spv

// ── Scene UBO ────────────────────────────────────────────────────────────────
struct SceneUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 lightSpaceMatrix;
    float3   lightPos;
    float    _pad0;
    float3   viewPos;
    float    _pad1;
};

[[vk::binding(0, 0)]]
ConstantBuffer<SceneUBO> scene;

[[vk::binding(1, 0)]]
Texture2D    diffuseTexture;
[[vk::binding(1, 0)]]
SamplerState diffuseSampler;

[[vk::binding(2, 0)]]
Texture2D    shadowMap;
[[vk::binding(2, 0)]]
SamplerState shadowSampler;

// ── Push constants ────────────────────────────────────────────────────────────
struct LightingPushConstants
{
    float4x4 model;
};

[[vk::push_constant]]
LightingPushConstants pc;

// ── Vertex shader ─────────────────────────────────────────────────────────────
struct VSIn
{
    [[vk::location(0)]] float3 position  : POSITION;
    [[vk::location(1)]] float3 normal    : NORMAL;
    [[vk::location(2)]] float2 texCoords : TEXCOORD0;
};

struct VSOut
{
    float4 clipPos          : SV_Position;
    float3 fragPos          : TEXCOORD0;   // world-space position
    float3 normal           : TEXCOORD1;   // world-space normal
    float2 texCoords        : TEXCOORD2;
    float4 fragPosLightSpace: TEXCOORD3;   // clip-space from light's POV
};

[shader("vertex")]
VSOut vertMain(VSIn vsIn)
{
    VSOut vsOut;

    float4 worldPos   = mul(pc.model, float4(vsIn.position, 1.0));
    vsOut.fragPos     = worldPos.xyz;

    // Normal matrix = transpose(inverse(model)) — avoids non-uniform scale artefacts.
    // For best performance pre-compute this on the CPU and pass it in a UBO.
    float3x3 normalMatrix = transpose((float3x3)((float3x4)pc.model)); // simplified; see note
    vsOut.normal      = normalize(mul(normalMatrix, vsIn.normal));

    vsOut.texCoords   = vsIn.texCoords;

    // Transform to light clip-space for shadow lookup in the fragment shader.
    vsOut.fragPosLightSpace = mul(scene.lightSpaceMatrix, worldPos);

    vsOut.clipPos     = mul(scene.projection, mul(scene.view, worldPos));
    return vsOut;
}

// ── Shadow helper ─────────────────────────────────────────────────────────────
//
// Returns 0.0  →  fully lit
//         1.0  →  fully in shadow
//
// Improvements applied vs. the raw LearnOpenGL baseline:
//   1. Slope-scaled bias       — reduces acne on angled surfaces
//   2. GL_CLAMP_TO_BORDER      — done via sampler, not in shader; but we still
//                                 guard against z > 1.0 for the far-plane case
//   3. 3×3 PCF kernel          — soft, anti-aliased shadow edges

float ShadowCalculation(float4 fragPosLightSpace, float3 normal, float3 lightDir)
{
    // ── 1. Perspective divide → NDC ─────────────────────────────────────────
    // For an orthographic light projection w == 1, so this is a no-op, but it
    // keeps the code correct for spot-light / perspective shadow maps too.
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // ── 2. NDC → UV space ───────────────────────────────────────────────────
    // Vulkan NDC X and Y are already in [-1, 1]; Z is in [0, 1] (unlike OpenGL's
    // [-1, 1]).  So we only need to remap XY to [0, 1].
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    // projCoords.z stays as-is — it's already in [0, 1] in Vulkan.

    // ── 3. Early-out: beyond the light's far plane ──────────────────────────
    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // ── 4. Slope-scaled bias ─────────────────────────────────────────────────
    // max() clamps to avoid over-biasing on surfaces facing the light directly.
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // ── 5. 3×3 PCF kernel ────────────────────────────────────────────────────
    // Sample the surrounding 9 texels and average the shadow results.
    float shadow    = 0.0;
    float2 texelSize;
    shadowMap.GetDimensions(texelSize.x, texelSize.y); // equivalent to textureSize()
    texelSize = 1.0 / texelSize;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset     = float2(x, y) * texelSize;
            float  pcfDepth   = shadowMap.Sample(shadowSampler, projCoords.xy + offset).r;
            shadow           += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

// ── Fragment shader ───────────────────────────────────────────────────────────
struct FSOut
{
    [[vk::location(0)]] float4 color : SV_Target;
};

[shader("fragment")]
FSOut fragMain(VSOut fsIn)
{
    FSOut fsOut;

    float3 albedo   = diffuseTexture.Sample(diffuseSampler, fsIn.texCoords).rgb;
    float3 normal   = normalize(fsIn.normal);
    float3 lightDir = normalize(scene.lightPos - fsIn.fragPos);
    float3 viewDir  = normalize(scene.viewPos  - fsIn.fragPos);
    float3 halfway  = normalize(lightDir + viewDir);

    float3 lightColor = float3(1.0, 1.0, 1.0);

    // Ambient — not modulated by shadow (indirect / bounce light approximation)
    float3 ambient  = 0.15 * lightColor;

    // Diffuse (Lambertian)
    float  diff     = max(dot(normal, lightDir), 0.0);
    float3 diffuse  = diff * lightColor;

    // Specular (Blinn-Phong)
    float  spec     = pow(max(dot(normal, halfway), 0.0), 64.0);
    float3 specular = spec * lightColor;

    // Shadow factor  [0 = lit, 1 = shadowed]
    float shadow = ShadowCalculation(fsIn.fragPosLightSpace, normal, lightDir);

    float3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * albedo;

    fsOut.color = float4(lighting, 1.0);
    return fsOut;
}
```

---

## Vulkan C++ skeleton (`shadow_mapping.cpp`)

Covers render pass configuration, pipeline layouts, descriptor set layout, the shadow sampler, the per-frame render loop, and light-space matrix construction. Device, swapchain, buffer, and image creation are omitted.

```cpp
// shadow_mapping.cpp  —  Vulkan-hpp RAII skeleton
// Covers: render pass structure, pipeline setup notes, and the two-pass
// render loop.  Resource creation (device, swapchain, buffers, images) is
// intentionally omitted per the task description.

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t SHADOW_MAP_SIZE = 1024;

// ─────────────────────────────────────────────────────────────────────────────
// Push constant structs — must match the Slang declarations exactly
// ─────────────────────────────────────────────────────────────────────────────
struct ShadowPushConstants
{
    glm::mat4 lightSpaceMatrix;
    glm::mat4 model;
};

struct LightingPushConstants
{
    glm::mat4 model;
};

// ─────────────────────────────────────────────────────────────────────────────
// SceneUBO — mirrors lighting_pass.slang's SceneUBO
// ─────────────────────────────────────────────────────────────────────────────
struct SceneUBO
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 lightPos;  float pad0{};
    glm::vec3 viewPos;   float pad1{};
};

// ─────────────────────────────────────────────────────────────────────────────
// Render pass configuration notes
// ─────────────────────────────────────────────────────────────────────────────
//
// You need TWO render passes (or one with two subpasses — two separate passes
// is simpler to reason about):
//
// Pass 1 — Shadow pass
//   Attachments : depth only (vk::Format::eD32Sfloat, SHADOW_MAP_SIZE²)
//   Load  op    : eClear   (clear depth to 1.0)
//   Store op    : eStore   (keep for sampling in pass 2)
//   Final layout: eShaderReadOnlyOptimal
//   Cull mode   : eFront   (peter-panning fix)
//   Colour write: none
//
// Pass 2 — Lighting pass
//   Attachments : colour (swapchain format) + depth (screen-res)
//   The shadow map image transitions from eShaderReadOnlyOptimal → stays there;
//   insert a vk::ImageMemoryBarrier between the two passes:
//
//     srcStageMask  = eLateFragmentTests
//     srcAccessMask = eDepthStencilAttachmentWrite
//     dstStageMask  = eFragmentShader
//     dstAccessMask = eShaderRead
//     oldLayout / newLayout = eShaderReadOnlyOptimal (if you used eStore above)

// ─────────────────────────────────────────────────────────────────────────────
// Pipeline layout helpers
// ─────────────────────────────────────────────────────────────────────────────

// Shadow pass pipeline layout
//   - Push constant range: ShadowPushConstants (VS only)
//   - No descriptor sets needed (no textures in the depth pass)
inline vk::raii::PipelineLayout makeShadowPipelineLayout(const vk::raii::Device& device)
{
    vk::PushConstantRange pcRange{
        vk::ShaderStageFlagBits::eVertex,
        0,
        sizeof(ShadowPushConstants)
    };

    vk::PipelineLayoutCreateInfo ci{};
    ci.setPushConstantRanges(pcRange);
    return device.createPipelineLayout(ci);
}

// Lighting pass pipeline layout
//   - Push constant range: LightingPushConstants (VS only)
//   - Descriptor set 0: SceneUBO + diffuseTexture + shadowMap
inline vk::raii::PipelineLayout makeLightingPipelineLayout(
    const vk::raii::Device&              device,
    const vk::raii::DescriptorSetLayout& dsl)
{
    vk::PushConstantRange pcRange{
        vk::ShaderStageFlagBits::eVertex,
        0,
        sizeof(LightingPushConstants)
    };

    vk::DescriptorSetLayout dslHandle = *dsl;

    vk::PipelineLayoutCreateInfo ci{};
    ci.setSetLayouts(dslHandle);
    ci.setPushConstantRanges(pcRange);
    return device.createPipelineLayout(ci);
}

// Descriptor set layout for the lighting pass
//   binding 0 → SceneUBO            (uniform buffer,          vertex + fragment)
//   binding 1 → diffuseTexture      (combined image sampler,  fragment)
//   binding 2 → shadowMap           (combined image sampler,  fragment)
inline vk::raii::DescriptorSetLayout makeLightingDSL(const vk::raii::Device& device)
{
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{{
        { 0, vk::DescriptorType::eUniformBuffer,        1,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eCombinedImageSampler, 1,
          vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eCombinedImageSampler, 1,
          vk::ShaderStageFlagBits::eFragment },
    }};

    vk::DescriptorSetLayoutCreateInfo ci{};
    ci.setBindings(bindings);
    return device.createDescriptorSetLayout(ci);
}

// Shadow-map sampler
//   - Nearest filtering (crisp depth reads)
//   - ClampToBorder with white border → outside UV range returns depth 1.0
//     so fragments outside the light frustum are never shadowed
inline vk::raii::Sampler makeShadowSampler(const vk::raii::Device& device)
{
    vk::SamplerCreateInfo ci{};
    ci.magFilter    = vk::Filter::eNearest;
    ci.minFilter    = vk::Filter::eNearest;
    ci.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    ci.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    ci.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    ci.borderColor  = vk::BorderColor::eFloatOpaqueWhite; // depth = 1.0 outside frustum
    return device.createSampler(ci);
}

// ─────────────────────────────────────────────────────────────────────────────
// Render loop (per-frame)
// ─────────────────────────────────────────────────────────────────────────────

struct FrameResources
{
    // Populated during initialisation — not shown here
    vk::raii::CommandBuffer cmd              { nullptr };

    // Shadow pass
    vk::raii::Framebuffer    shadowFramebuffer { nullptr }; // depth-only
    vk::raii::RenderPass     shadowRenderPass  { nullptr };
    vk::raii::Pipeline       shadowPipeline    { nullptr };
    vk::raii::PipelineLayout shadowLayout      { nullptr };

    // Lighting pass
    vk::raii::Framebuffer    lightingFramebuffer { nullptr };
    vk::raii::RenderPass     lightingRenderPass  { nullptr };
    vk::raii::Pipeline       lightingPipeline    { nullptr };
    vk::raii::PipelineLayout lightingLayout      { nullptr };

    vk::raii::DescriptorSet lightingDescriptorSet { nullptr };

    // Shadow map image/view (created elsewhere)
    vk::Image    shadowMapImage{};
    vk::Extent2D swapchainExtent{};
};

void renderFrame(
    const FrameResources&  res,
    const SceneUBO&        sceneData,
    const std::vector<std::pair<glm::mat4, /* mesh */ void*>>& objects)
{
    auto& cmd = res.cmd;
    cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    // ═══════════════════════════════════════════════════════════════════════
    // PASS 1 — Shadow map generation
    // ═══════════════════════════════════════════════════════════════════════
    {
        std::array<vk::ClearValue, 1> clears;
        clears[0].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        vk::RenderPassBeginInfo rpbi{};
        rpbi.renderPass        = *res.shadowRenderPass;
        rpbi.framebuffer       = *res.shadowFramebuffer;
        rpbi.renderArea.extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
        rpbi.setClearValues(clears);

        cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *res.shadowPipeline);

        // Viewport / scissor — shadow map resolution
        vk::Viewport vp{ 0, 0,
            static_cast<float>(SHADOW_MAP_SIZE),
            static_cast<float>(SHADOW_MAP_SIZE),
            0.0f, 1.0f };
        cmd.setViewport(0, vp);
        cmd.setScissor(0, vk::Rect2D{ {0,0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} });

        for (auto& [model, mesh] : objects)
        {
            ShadowPushConstants pc{
                sceneData.lightSpaceMatrix,
                model
            };
            cmd.pushConstants<ShadowPushConstants>(
                *res.shadowLayout,
                vk::ShaderStageFlagBits::eVertex,
                0, pc);

            // drawMesh(cmd, mesh);  ← your mesh draw call here
        }

        cmd.endRenderPass();
    }

    // ── Barrier: depth attachment write → shader read ───────────────────────
    {
        vk::ImageMemoryBarrier barrier{};
        barrier.srcAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        barrier.dstAccessMask    = vk::AccessFlagBits::eShaderRead;
        barrier.oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        barrier.newLayout        = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.image            = res.shadowMapImage;
        barrier.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eFragmentShader,
            {}, {}, {}, barrier);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PASS 2 — Lighting with PCF shadow lookup
    // ═══════════════════════════════════════════════════════════════════════
    {
        std::array<vk::ClearValue, 2> clears;
        clears[0].color        = vk::ClearColorValue{ std::array<float,4>{ 0.1f, 0.1f, 0.1f, 1.0f } };
        clears[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        vk::RenderPassBeginInfo rpbi{};
        rpbi.renderPass        = *res.lightingRenderPass;
        rpbi.framebuffer       = *res.lightingFramebuffer;
        rpbi.renderArea.extent = res.swapchainExtent;
        rpbi.setClearValues(clears);

        cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *res.lightingPipeline);

        vk::Viewport vp{
            0, 0,
            static_cast<float>(res.swapchainExtent.width),
            static_cast<float>(res.swapchainExtent.height),
            0.0f, 1.0f };
        cmd.setViewport(0, vp);
        cmd.setScissor(0, vk::Rect2D{ {0,0}, res.swapchainExtent });

        // Bind UBO + textures (descriptor set 0)
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *res.lightingLayout,
            0, *res.lightingDescriptorSet, {});

        for (auto& [model, mesh] : objects)
        {
            LightingPushConstants pc{ model };
            cmd.pushConstants<LightingPushConstants>(
                *res.lightingLayout,
                vk::ShaderStageFlagBits::eVertex,
                0, pc);

            // drawMesh(cmd, mesh);  ← your mesh draw call here
        }

        cmd.endRenderPass();
    }

    // ── Barrier back: shader read → depth attachment (next frame) ───────────
    // Usually handled by the render pass's finalLayout / nextLayout.
    // If you use eDepthStencilAttachmentOptimal as the shadow pass finalLayout
    // the driver handles the transition automatically on the next beginRenderPass.

    cmd.end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Light space matrix construction
// ─────────────────────────────────────────────────────────────────────────────
//
// Requires:
//   #define GLM_FORCE_DEPTH_ZERO_TO_ONE   // before any GLM include
//   #define GLM_FORCE_RADIANS

glm::mat4 buildLightSpaceMatrix(glm::vec3 lightPos)
{
    // Orthographic frustum sized to cover the scene.
    // Tighten the bounds to your scene for best shadow map precision.
    float nearPlane = 1.0f, farPlane = 7.5f;
    glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, nearPlane, farPlane);

    glm::mat4 lightView = glm::lookAt(
        lightPos,
        glm::vec3(0.0f),              // scene centre
        glm::vec3(0.0f, 1.0f, 0.0f));

    return lightProjection * lightView;
}
```

---

## Compilation

```sh
# Shadow pass
slangc shadow_pass.slang   -profile glsl_450 -entry vertMain -o shadow_pass_vert.spv
slangc shadow_pass.slang   -profile glsl_450 -entry fragMain -o shadow_pass_frag.spv

# Lighting pass
slangc lighting_pass.slang -profile glsl_450 -entry vertMain -o lighting_pass_vert.spv
slangc lighting_pass.slang -profile glsl_450 -entry fragMain -o lighting_pass_frag.spv
```

Or target DXIL with `-profile sm_6_0` if you're on D3D12.
