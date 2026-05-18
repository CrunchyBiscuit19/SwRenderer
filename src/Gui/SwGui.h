#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED
#include <imgui.h>
#define NOMINMAX  // imfilebrowser.h contains windows.h
#include <Renderer/SwRendererContext.h>
#include <Resource/SwDescriptor.h>
#include <imfilebrowser.h>
#include <functional>

class SwGui {
private:
    enum class SwGuiComponent { Scene, Camera, Stats, Controls };

    static SwRendererContext sRendererContext;

    SwDescriptorPool mDescriptorPool;
    bool mCollapsed;
    std::unordered_map<SwGuiComponent, std::function<void()>> mGuiComponents;
    ImGui::FileBrowser mSelectAssetsFileBrowser;
    ImGui::FileBrowser mSelectSkyboxFileBrowser;

    void createDockSpace();
    void createRendererOptionsWindow() const;

public:

    SwGui();

    static void init(SwRendererContext guiContext);

    void initialize();

    void update();

    ~SwGui();
};
