#include <Gui/SwGui.h>
#include <Renderer/SwEvents.h>
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

    // TODO implement all passes first

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const SDL_Keymod modState = SDL_GetModState();
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);

        if (keyState[SDL_SCANCODE_G] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mCollapsed = !mCollapsed;
        }

        if (keyState[SDL_SCANCODE_T] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            sRendererContext.mScene->changePickOperation(); 
        }

        if ((modState & KMOD_CTRL) && keyState[SDL_SCANCODE_I] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mSelectAssetsFileBrowser.Open();
            sRendererContext.mScene->getCamera().setRelativeMode(SDL_FALSE);
        }
    });
}

void SwGui::update() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    createDockSpace();
    createRendererOptionsWindow();
    sRendererContext.mScene->generatePickFrame();

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

void SwGui::createRendererOptionsWindow() const {
    if (mCollapsed) return;
    if (!ImGui::Begin("Renderer Options", nullptr, ImGuiWindowFlags_NoDecoration)) return;
    if (ImGui::IsWindowCollapsed()) return;

    for (auto& component : mGuiComponents) {
        if (!ImGui::CollapsingHeader(magic_enum::enum_name(component.first).data())) return;
        component.second();
    }

    ImGui::End();
}

SwGui::~SwGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
}
