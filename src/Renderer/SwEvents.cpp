#include "SwEvents.h"

void SwEvents::addEventCallback(const std::function<void(SDL_Event& e)>& inputCallback) { mEventCallbacks.emplace_back(inputCallback); }

void SwEvents::executeEventCallbacks(SDL_Event& e) const {
    for (const auto& inputCallback : mEventCallbacks) {
        inputCallback(e);
    }
}
