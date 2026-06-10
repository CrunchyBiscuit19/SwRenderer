#pragma once

#include <Data/SwLight.h>
#include <Scene/SwSystem.h>

#include <vector>

namespace SwLighting {

class System : public SwSystem {
private:
    std::vector<SwLight::Data> mAssetLights;  // per-instance records emitted by asset SwLightNodes during regen
    std::vector<SwLight> mGlobalLights;       // STUB: editor/default lights with no owning asset — empty for now

    void initializeResources() override;
    void initializePasses() override;

public:
    System(SwScene& scene);

    inline std::vector<SwLight::Data>& getAssetLights() { return mAssetLights; }
    inline std::vector<SwLight>& getGlobalLights() { return mGlobalLights; }
};

}  // namespace SwLighting
