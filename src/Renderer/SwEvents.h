#pragma once

#include <SDL3/SDL_events.h>

#include <functional>

class SwEvents {
private:
    std::vector<std::function<void(SDL_Event& e)>> mEventCallbacks;

public:
    void addEventCallback(const std::function<void(SDL_Event& e)>& inputCallback);

    void executeEventCallbacks(SDL_Event& e) const;
};