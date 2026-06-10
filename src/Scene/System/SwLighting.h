#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>

#include <vector>

namespace SwLighting {

struct Resources {
    std::vector<SwLight::Data> mAssetLights;  // per-instance records emitted by asset SwLightNodes during regen
    std::vector<SwLight> mGlobalLights;       // STUB: editor/default lights with no owning asset — empty for now
};

class System : public SwSystem {
private:
    Resources mResources;

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    inline Resources& getResources() { return mResources; }
    inline std::vector<SwLight::Data>& getAssetLights() { return mResources.mAssetLights; }
    inline std::vector<SwLight>& getGlobalLights() { return mResources.mGlobalLights; }
};

}  // namespace SwLighting
