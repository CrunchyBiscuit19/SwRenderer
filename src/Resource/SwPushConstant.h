#pragma once

#include <vulkan/vulkan.hpp>

template <typename T>
struct SwPC {
    static vk::PushConstantRange getRange(std::uint32_t offset = 0) { return {T::sStages, offset, sizeof(T)}; }
};

