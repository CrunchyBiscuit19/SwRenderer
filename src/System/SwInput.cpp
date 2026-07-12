#include <System/SwInput.h>

#include <Renderer/SwRenderer.h>
#include <SDL3/SDL_keyboard.h>
#include <nlohmann/json.hpp>
#include <quill/LogMacros.h>

#include <array>
#include <fstream>

namespace {
std::optional<SDL_Scancode> scancodeFromName(const std::string& name) {
    const SDL_Scancode code = SDL_GetScancodeFromName(name.c_str());
    if (code == SDL_SCANCODE_UNKNOWN) return std::nullopt;
    return code;
}

std::optional<std::uint8_t> mouseButtonFromName(const std::string& name) {
    if (name == "LEFT") return SDL_BUTTON_LEFT;
    if (name == "RIGHT") return SDL_BUTTON_RIGHT;
    if (name == "MIDDLE") return SDL_BUTTON_MIDDLE;
    return std::nullopt;
}

SDL_Keymod modFromName(const std::string& name) {
    if (name == "CTRL") return SDL_KMOD_CTRL;
    if (name == "ALT") return SDL_KMOD_ALT;
    if (name == "SHIFT") return SDL_KMOD_SHIFT;
    if (name == "LSHIFT") return SDL_KMOD_LSHIFT;
    if (name == "RSHIFT") return SDL_KMOD_RSHIFT;
    return SDL_KMOD_NONE;
}

// Each required modifier group (ctrl, shift, alt, gui) is satisfied when either of its left or right
// keys is held, so a binding never demands both physical keys of a pair at once.
bool modsSatisfied(SDL_Keymod required) {
    const SDL_Keymod state = SDL_GetModState();
    if ((required & SDL_KMOD_CTRL) && !(state & SDL_KMOD_CTRL)) return false;
    if ((required & SDL_KMOD_SHIFT) && !(state & SDL_KMOD_SHIFT)) return false;
    if ((required & SDL_KMOD_ALT) && !(state & SDL_KMOD_ALT)) return false;
    if ((required & SDL_KMOD_GUI) && !(state & SDL_KMOD_GUI)) return false;
    return true;
}
}  // namespace

SwInput::System::System(SwScene& scene) : SwSystem(scene) {}

void SwInput::System::initializeResources() {
    loadBindings();
    SwRenderer::sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void { onEvent(e); });
}

void SwInput::System::initializePasses() {}

void SwInput::System::loadBindings() {
    std::ifstream file(KEYBINDINGS_FILE);
    if (!file.is_open()) {
        LOG_WARNING(SwRenderer::sRendererContext.mLogger->getQuillPtr(), "Keybindings file not found at {}, using defaults", KEYBINDINGS_FILE.string());
        buildDefaults();
        return;
    }

    nlohmann::ordered_json json;
    try {
        file >> json;
    } catch (const std::exception& ex) {
        LOG_WARNING(SwRenderer::sRendererContext.mLogger->getQuillPtr(), "Failed to parse keybindings file ({}), using defaults", ex.what());
        buildDefaults();
        return;
    }

    if (json.contains("settings")) {
        const auto& settings = json["settings"];
        mSettings.mLookSensitivity = settings.value("look_sensitivity", mSettings.mLookSensitivity);
        mSettings.mScrollSpeed = settings.value("scroll_speed", mSettings.mScrollSpeed);
        mSettings.mMoveSpeed = settings.value("move_speed", mSettings.mMoveSpeed);
    }

    if (!json.contains("actions")) {
        buildDefaults();
        return;
    }

    for (const auto& [name, def] : json["actions"].items()) {
        SwBinding binding;
        if (def.contains("key")) {
            binding.mKey = scancodeFromName(def["key"].get<std::string>());
            if (!binding.mKey.has_value())
                LOG_WARNING(SwRenderer::sRendererContext.mLogger->getQuillPtr(), "Unknown key '{}' for action '{}'", def["key"].get<std::string>(), name);
        }
        if (def.contains("mouse")) binding.mMouseButton = mouseButtonFromName(def["mouse"].get<std::string>());
        if (def.contains("mods"))
            for (const auto& mod : def["mods"]) binding.mMods = static_cast<SDL_Keymod>(binding.mMods | modFromName(mod.get<std::string>()));
        binding.mEdge = def.value("trigger", std::string("hold")) == "press";
        binding.mDescription = def.value("description", std::string());

        mActionOrder.push_back(name);
        mActions[name] = std::move(binding);
    }
}

void SwInput::System::buildDefaults() {
    struct DefaultBinding {
        std::string_view mName;
        std::optional<SDL_Scancode> mKey;
        std::optional<std::uint8_t> mMouse;
        SDL_Keymod mMods;
        bool mEdge;
        std::string_view mDescription;
    };
    static const std::array<DefaultBinding, 14> defaults{{
        {CAMERA_FORWARD, SDL_SCANCODE_W, std::nullopt, SDL_KMOD_NONE, false, ""},
        {CAMERA_BACK, SDL_SCANCODE_S, std::nullopt, SDL_KMOD_NONE, false, ""},
        {CAMERA_LEFT, SDL_SCANCODE_A, std::nullopt, SDL_KMOD_NONE, false, ""},
        {CAMERA_RIGHT, SDL_SCANCODE_D, std::nullopt, SDL_KMOD_NONE, false, ""},
        {CAMERA_UP, SDL_SCANCODE_W, std::nullopt, SDL_KMOD_LSHIFT, false, ""},
        {CAMERA_DOWN, SDL_SCANCODE_S, std::nullopt, SDL_KMOD_LSHIFT, false, ""},
        {CAMERA_TOGGLE_MODE, SDL_SCANCODE_C, std::nullopt, SDL_KMOD_NONE, true, "Change Camera Mode"},
        {CAMERA_LOOK, std::nullopt, SDL_BUTTON_RIGHT, SDL_KMOD_NONE, true, "Enter / Leave Window"},
        {TOGGLE_FULLSCREEN, SDL_SCANCODE_RETURN, std::nullopt, SDL_KMOD_ALT, true, "Toggle Borderless Fullscreen"},
        {TOGGLE_GUI, SDL_SCANCODE_G, std::nullopt, SDL_KMOD_NONE, true, "Toggle GUI"},
        {CYCLE_TRANSFORM, SDL_SCANCODE_T, std::nullopt, SDL_KMOD_NONE, true, "Switch Transform Mode"},
        {IMPORT_ASSET, SDL_SCANCODE_I, std::nullopt, SDL_KMOD_CTRL, true, "Import Asset"},
        {DELETE_INSTANCE, SDL_SCANCODE_DELETE, std::nullopt, SDL_KMOD_NONE, true, "Delete Clicked Instance"},
        {SELECT_OBJECT, std::nullopt, SDL_BUTTON_LEFT, SDL_KMOD_NONE, false, "Select / Deselect Object"},
    }};

    for (const auto& def : defaults) {
        SwBinding binding{def.mKey, def.mMouse, def.mMods, def.mEdge, std::string(def.mDescription)};
        mActionOrder.emplace_back(def.mName);
        mActions[std::string(def.mName)] = std::move(binding);
    }
}

bool SwInput::System::bindingHeld(const SwBinding& binding) const {
    if (!modsSatisfied(binding.mMods)) return false;
    if (binding.mKey.has_value()) {
        const bool* keyState = SDL_GetKeyboardState(nullptr);
        if (!keyState[*binding.mKey]) return false;
    }
    if (binding.mMouseButton.has_value()) {
        if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(*binding.mMouseButton))) return false;
    }
    return binding.mKey.has_value() || binding.mMouseButton.has_value();
}

void SwInput::System::onEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        mMouseDelta.x += e.motion.xrel;
        mMouseDelta.y += e.motion.yrel;
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL) mWheelDelta += e.wheel.y;

    const bool keyDown = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat;
    const bool mouseDown = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    if (!keyDown && !mouseDown) return;

    for (const auto& [name, binding] : mActions) {
        if (!binding.mEdge) continue;
        if (!modsSatisfied(binding.mMods)) continue;
        if (keyDown && binding.mKey.has_value() && e.key.scancode == *binding.mKey) mTriggered.insert(name);
        else if (mouseDown && binding.mMouseButton.has_value() && e.button.button == *binding.mMouseButton) mTriggered.insert(name);
    }
}

void SwInput::System::beginFrame() {
    mTriggered.clear();
    mMouseDelta = glm::vec2(0.f);
    mWheelDelta = 0.f;
}

bool SwInput::System::isActive(std::string_view action) const {
    const auto it = mActions.find(std::string(action));
    return it != mActions.end() && bindingHeld(it->second);
}

bool SwInput::System::wasTriggered(std::string_view action) const { return mTriggered.contains(std::string(action)); }

const SwInput::SwBinding* SwInput::System::getBinding(std::string_view action) const {
    const auto it = mActions.find(std::string(action));
    return it != mActions.end() ? &it->second : nullptr;
}

std::string SwInput::System::bindingLabel(std::string_view action) const {
    const SwBinding* binding = getBinding(action);
    if (binding == nullptr) return "Unbound";

    std::string label;
    if (binding->mMods & SDL_KMOD_CTRL) label += "Ctrl + ";
    if (binding->mMods & SDL_KMOD_ALT) label += "Alt + ";
    if (binding->mMods & SDL_KMOD_SHIFT) label += "Shift + ";

    if (binding->mKey.has_value()) {
        label += SDL_GetScancodeName(*binding->mKey);
    } else if (binding->mMouseButton.has_value()) {
        switch (*binding->mMouseButton) {
            case SDL_BUTTON_LEFT:
                label += "Left Click";
                break;
            case SDL_BUTTON_RIGHT:
                label += "Right Click";
                break;
            case SDL_BUTTON_MIDDLE:
                label += "Middle Click";
                break;
            default:
                label += "Mouse";
        }
    }
    return label;
}
