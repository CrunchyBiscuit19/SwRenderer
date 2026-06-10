#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>

#include <vector>

namespace SwLighting {

struct Resources {
    SwSunlight mSunlight;                     // Single scene-global directional light, uploaded per frame via PerFrameData
    std::vector<SwLight::Data> mAssetLights;  // Per-instance records emitted by asset SwLightNodes during regen
    std::vector<SwLight> mGlobalLights;       // Editor lights with no owning asset — STUB empty for now
};

class System : public SwSystem {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
    inline SwSunlight& getSunlight() { return mResources.mSunlight; }
    inline std::vector<SwLight::Data>& getAssetLights() { return mResources.mAssetLights; }
    inline std::vector<SwLight>& getGlobalLights() { return mResources.mGlobalLights; }
};

}  // namespace SwLighting
