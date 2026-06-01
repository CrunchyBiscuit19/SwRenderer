#pragma once

#include <glm/glm.hpp>

class SwInstance {
public:
    struct Data {
        glm::mat4 mTransformMatrix{1.0f};

        Data() = default;
        Data(glm::mat4 transform) : mTransformMatrix(std::move(transform)) {}
    };

private:
    static std::uint32_t sLatestInstanceId;

    std::uint32_t mAssetId;
    std::uint32_t mId;
    bool mDelete{false};

    Data mData;

public:
    SwInstance(std::uint32_t assetId, Data data = Data());

    SwInstance(SwInstance&&) noexcept = default;
    SwInstance& operator=(SwInstance&&) noexcept = default;

    SwInstance(const SwInstance&) = delete;
    SwInstance& operator=(const SwInstance&) = delete;

    inline std::uint32_t getId() const { return mId; }
    inline Data& getData() { return mData; }
    inline std::uint32_t getAssetId() const { return mAssetId; }
    inline bool isMarkedDelete() const { return mDelete; }

    inline void markDelete() { mDelete = true; }
};