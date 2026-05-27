#pragma once

#include <Renderer/SwRendererContext.h>

class SwScene;

class SwSystem {
public:    
    class Resizable {
    protected:
        virtual void reInitializeOnResize() = 0;

    public:
        virtual void resize() = 0;
    };

protected:
    SwScene& mScene;

    virtual void initializeResources() = 0;
    virtual void initializePasses() = 0;

public:
    static SwRendererContext sRendererContext;

    SwSystem(SwScene& scene);

    void initialize();
};
