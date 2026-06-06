#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED
#include <imgui.h>
#define NOMINMAX  // imfilebrowser.h contains windows.h
#include <Renderer/SwRendererContext.h>
#include <Resource/SwDescriptor.h>
#include <Scene/SwSystem.h>
#include <imfilebrowser.h>
#include <functional>

class SwScene;

class SwGui : public SwSystem {
private:
    enum class SwGuiComponent { Scene, Camera, Stats, Controls };

    static constexpr ImVec4 IMGUI_HEADER_GREEN{0.22f, 0.69f, 0.502f, 1.0f};
    static constexpr ImVec4 IMGUI_BUTTON_RED{0.66f, 0.16f, 0.16f, 1.0f};


    SwDescriptorPool mDescriptorPool;
    bool mCollapsed{false};
    std::unordered_map<SwGuiComponent, std::function<void()>> mGuiComponents;
    ImGui::FileBrowser mSelectAssetsFileBrowser;
    ImGui::FileBrowser mSelectSkyboxFileBrowser;

    void createDockSpace();
    void createOptionsWindow() const;

    void initializeResources() override;
    void initializePasses() override;

public:

    SwGui(SwScene& scene);

    void refresh() override;
    void refreshDynamicDependencies() override;

    ~SwGui();
};
