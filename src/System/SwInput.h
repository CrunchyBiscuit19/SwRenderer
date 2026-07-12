#pragma once

#include <Scene/SwSystem.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SwInput {
static const std::filesystem::path KEYBINDINGS_FILE{KEYBINDINGS_PATH};

inline constexpr std::string_view CAMERA_FORWARD{"camera_forward"};
inline constexpr std::string_view CAMERA_BACK{"camera_back"};
inline constexpr std::string_view CAMERA_LEFT{"camera_left"};
inline constexpr std::string_view CAMERA_RIGHT{"camera_right"};
inline constexpr std::string_view CAMERA_UP{"camera_up"};
inline constexpr std::string_view CAMERA_DOWN{"camera_down"};
inline constexpr std::string_view CAMERA_TOGGLE_MODE{"camera_toggle_mode"};
inline constexpr std::string_view CAMERA_LOOK{"camera_look"};
inline constexpr std::string_view TOGGLE_FULLSCREEN{"toggle_fullscreen"};
inline constexpr std::string_view TOGGLE_GUI{"toggle_gui"};
inline constexpr std::string_view CYCLE_TRANSFORM{"cycle_transform"};
inline constexpr std::string_view IMPORT_ASSET{"import_asset"};
inline constexpr std::string_view DELETE_INSTANCE{"delete_instance"};
inline constexpr std::string_view SELECT_OBJECT{"select_object"};

struct SwBinding {
    std::optional<SDL_Scancode> mKey;
    std::optional<std::uint8_t> mMouseButton;  // SDL_BUTTON_LEFT / _RIGHT / _MIDDLE
    SDL_Keymod mMods{SDL_KMOD_NONE};
    bool mEdge{false};  // true for "press" (key-down edge), false for "hold"
    std::string mDescription;
};

struct SwInputSettings {
    float mLookSensitivity{200.f};
    float mScrollSpeed{0.2f};
    float mMoveSpeed{0.1f};
};

class System : public SwSystem {
private:
    std::unordered_map<std::string, SwBinding> mActions;
    std::vector<std::string> mActionOrder;  // preserves config order for the GUI controls list
    std::unordered_set<std::string> mTriggered;
    glm::vec2 mMouseDelta{0.f};
    float mWheelDelta{0.f};
    SwInputSettings mSettings;

    void loadBindings();
    void buildDefaults();
    void onEvent(const SDL_Event& e);
    bool bindingHeld(const SwBinding& binding) const;

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    void beginFrame();

    bool isActive(std::string_view action) const;
    bool wasTriggered(std::string_view action) const;
    glm::vec2 getMouseDelta() const { return mMouseDelta; }
    float getWheelDelta() const { return mWheelDelta; }

    std::string bindingLabel(std::string_view action) const;
    const std::vector<std::string>& getActionOrder() const { return mActionOrder; }
    const SwBinding* getBinding(std::string_view action) const;

    float lookSensitivity() const { return mSettings.mLookSensitivity; }
    float scrollSpeed() const { return mSettings.mScrollSpeed; }
    float moveSpeed() const { return mSettings.mMoveSpeed; }
};
}  // namespace SwInput
