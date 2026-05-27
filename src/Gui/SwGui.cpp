#include <Gui/SwGui.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Scene/SwScene.h>
#include <fmt/core.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>
#include <magic_enum.hpp>
#include <ranges>
#include <vulkan/vulkan_raii.hpp>

SwRendererContext SwGui::sRendererContext{};

SwGui::SwGui() {}

void SwGui::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwGui::initialize() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(sRendererContext.mSwapchain->getWindow());

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &SwSwapchain::SRGB_FORMAT;
    pipelineRenderingCreateInfo.depthAttachmentFormat = SwSwapchain::DEPTH_FORMAT;

    std::array<SwPoolSizeRatio, 1> ratios{
        {vk::DescriptorType::eCombinedImageSampler, 1000},
    };
    mDescriptorPool = sRendererContext.mDescriptorAllocator->createDescriptorPool(ratios, 100);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = **sRendererContext.mInstance;
    initInfo.PhysicalDevice = **sRendererContext.mChosenGPU;
    initInfo.Device = **sRendererContext.mDevice;
    initInfo.Queue = **sRendererContext.mGraphicsQueue;
    initInfo.DescriptorPool = mDescriptorPool.getRawPool();
    initInfo.MinImageCount = SwSwapchain::NUM_SWAPCHAIN_IMAGES;
    initInfo.ImageCount = SwSwapchain::NUM_SWAPCHAIN_IMAGES;
    initInfo.UseDynamicRendering = true;
    initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
    ImGui_ImplVulkan_DestroyFontsTexture();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;

    // Hack solution of applying a gamma correction factor to reduce brightness
    ImGuiStyle& style = ImGui::GetStyle();
    constexpr float gamma = 2.2f;
    for (std::uint32_t i = 0; i < ImGuiCol_COUNT; i++) {
        ImVec4& col = style.Colors[i];
        col.x = pow(col.x, gamma);
        col.y = pow(col.y, gamma);
        col.z = pow(col.z, gamma);
    }

    mSelectAssetsFileBrowser = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_MultipleSelection | ImGuiFileBrowserFlags_ConfirmOnEnter, MODELS_PATH);
    mSelectAssetsFileBrowser.SetTitle("Select GLTF / GLB File");
    mSelectAssetsFileBrowser.SetTypeFilters({".glb", ".gltf"});

    mSelectSkyboxFileBrowser = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_SelectDirectory, SKYBOXES_PATH);
    mSelectSkyboxFileBrowser.SetTitle("Select Directory of Skybox Image");

    mGuiComponents[SwGuiComponent::Camera] = [this]() {
        ImGui::Text("Camera Mode: %s", magic_enum::enum_name(sRendererContext.mScene->getCamera().getMovementMode()).data());
        ImGui::Text("Mouse Mode: %s", (sRendererContext.mScene->getCamera().getRelativeMode() ? "RELATIVE" : "NORMAL"));
        glm::vec3 camPos = sRendererContext.mScene->getCamera().getPosition();
        ImGui::Text("Position: [%.1f, %.1f, %.1f]", camPos.x, camPos.y, camPos.z);
        ImGui::Text("Pitch & Yaw: [%.1f, %.1f]", sRendererContext.mScene->getCamera().getPitch(), sRendererContext.mScene->getCamera().getYaw());
        ImGui::Text("Speed: %.2f / %.2f", sRendererContext.mScene->getCamera().getSpeed(), SwCamera::MAX_CAMERA_SPEED);
    };
    mGuiComponents[SwGuiComponent::Scene] = [this]() {
        for (auto& asset : sRendererContext.mScene->getAssets() | std::views::values) {
            const auto name = asset.getName();
            ImGui::PushStyleColor(ImGuiCol_Header, static_cast<ImVec4>(IMGUI_HEADER_GREEN));
            if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button(fmt::format("Add Instance##{}", name).c_str())) {
                    asset.createInstance(sRendererContext.mScene->getCamera().getSpawnTransform());
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
                if (ImGui::Button(fmt::format("Delete Model##{}", name).c_str())) {
                    asset.markDelete();
                }
                ImGui::PopStyleColor();

                for (auto& instance : asset.getInstances()) {
                    if (ImGui::TreeNode(fmt::format("{}-{}", name, instance.getId()).c_str())) {
                        ImGui::PushID(fmt::format("{}-{}", name, instance.getId()).c_str());
                        glm::vec3 translation, rotation, scale;
                        ImGuizmo::DecomposeMatrixToComponents(
                            glm::value_ptr(instance.getDataAddress()->mTransformMatrix), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale)
                        );
                        for (std::uint32_t i = 0; i < 3; i++) {
                            rotation[i] = glm::radians(rotation[i]);
                        }
                        ImGui::InputFloat3("Translation", glm::value_ptr(translation), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::InputFloat3("Rotation", glm::value_ptr(rotation), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::InputFloat3("Scale", glm::value_ptr(scale), "%.3f", ImGuiInputTextFlags_ReadOnly);

                        ImGui::PopID();
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::PopStyleColor();
        }
        /*if (ImGui::CollapsingHeader("Sunlight", ImGuiTreeNodeFlags_DefaultOpen)) { // TODO introduce lighting later
            ImGui::ColorEdit3("Ambient Color", glm::value_ptr(sRendererContext.mScene.mPerspective.mData.ambientColor));
            ImGui::ColorEdit3("Sunlight Color", glm::value_ptr(sRendererContext.mScene.mPerspective.mData.sunlightColor));
            ImGui::SliderFloat3("Sunlight Direction", glm::value_ptr(sRendererContext.mScene.mPerspective.mData.sunlightDirection), 0.f, 10.f);
            ImGui::InputFloat("Sunlight Power", &sRendererContext.mScene.mPerspective.mData.sunlightDirection[3]);
        }*/
        if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Change Skybox")) {
                mSelectSkyboxFileBrowser.Open();
            }
            ImGui::SameLine();
            if (ImGui::Button("Toggle Skybox")) {
                sRendererContext.mScene->getSkyboxSystem().toggleActive();
            }
        }
        if (ImGui::CollapsingHeader("Culler", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Freeze Culling", &sRendererContext.mScene->getCullSystem().getFreezeRef());
        }
        mSelectSkyboxFileBrowser.Display();
        if (mSelectSkyboxFileBrowser.HasSelected()) {
            std::filesystem::path selectedSkyboxDir = mSelectSkyboxFileBrowser.GetSelected();
            sRendererContext.mScene->getSkyboxSystem().reinitializeOnUpdate(selectedSkyboxDir);
            mSelectSkyboxFileBrowser.ClearSelected();
        }
    };
    mGuiComponents[SwGuiComponent::Stats] = [this]() {
        ImGui::Text("VALIDATION MODE: %s", magic_enum::enum_name(SwRenderer::VALIDATION_MODE).data());
        ImGui::Text("FPS:  %.2f", 1000.f / sRendererContext.mStats->mFrameTime);
        ImGui::Text("Frame Time:  %.2fms", sRendererContext.mStats->mFrameTime);
        ImGui::Text("Draw Time:  %.2fms", sRendererContext.mStats->mDrawTime);
        ImGui::Text("Update Time: %.2fms", sRendererContext.mStats->mSceneUpdateTime);
        ImGui::Text("Draws: %i", sRendererContext.mStats->mDrawCallCount);
        ImGui::Text("Pre-Cull Render Instances: %i", sRendererContext.mStats->mPreCullRenderInstancesCount);
        ImGui::Text("Post-Cull Render Instances: %i", *static_cast<std::uint32_t*>(sRendererContext.mStats->mRenderInstancesCountBuffer.getMappedPointer()));
    };
    mGuiComponents[SwGuiComponent::Controls] = [this]() {
        if (!ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::Text("[G] Toggle GUI");
        ImGui::Text("[Alt + Enter] Toggle Borderless Fullscreen");
        ImGui::Text("[C] Change Camera Mode");
        ImGui::Text("[Mouse Scroll] Control Camera Speed");
        ImGui::Text("[Left Click] Select / Deselect Object");
        ImGui::Text("[Right Click] Enter / Leave Window");
        ImGui::Text("[Ctrl + I] Import Model");
        ImGui::Text("[T] Switch Transform Mode");
        ImGui::Text("[Del] Delete Clicked Instance");
    };

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const SDL_Keymod modState = SDL_GetModState();
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);

        if (keyState[SDL_SCANCODE_G] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mCollapsed = !mCollapsed;
        }

        if (keyState[SDL_SCANCODE_T] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            sRendererContext.mScene->getPickSystem().changePickOperation();
        }

        if ((modState & KMOD_CTRL) && keyState[SDL_SCANCODE_I] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mSelectAssetsFileBrowser.Open();
            sRendererContext.mScene->getCamera().setRelativeMode(SDL_FALSE);
        }
    });
}

void SwGui::perFrameUpdate() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    createDockSpace();
    createOptionsWindow();
    sRendererContext.mScene->getPickSystem().generatePickFrame();

    mSelectAssetsFileBrowser.Display();
    if (mSelectAssetsFileBrowser.HasSelected()) {
        auto selectedFiles = mSelectAssetsFileBrowser.GetMultiSelected();
        sRendererContext.mScene->loadAssets(selectedFiles);
        mSelectAssetsFileBrowser.ClearSelected();
    }

    ImGui::Render();
}

void SwGui::createDockSpace() {
    static ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_NoDockingOverCentralNode | ImGuiDockNodeFlags_PassthruCentralNode;
    static ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                          ImGuiWindowFlags_NoBackground;

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace Window", &mCollapsed, windowFlags);
    static ImGuiID mainDockSpace = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(mainDockSpace, ImVec2(0.0f, 0.0f), dockSpaceFlags);
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void SwGui::createOptionsWindow() const {
    if (mCollapsed) return;
    if (!ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoDecoration)) {
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowCollapsed()) {
        ImGui::End();
        return;
    }
    for (auto& component : mGuiComponents) {
        if (!ImGui::CollapsingHeader(magic_enum::enum_name(component.first).data())) continue;
        component.second();
    }
    ImGui::End();
}

SwGui::~SwGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
}
