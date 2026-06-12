#include <Renderer/SwHelper.h>
#include <cmath>

vk::Extent2D SwHelper::extent3dTo2d(vk::Extent3D extent3d) { return vk::Extent2D(extent3d.width, extent3d.height); }

std::uint32_t SwHelper::fastDivCeil(std::uint32_t x, std::uint32_t y) { return (x + y - 1) / y; }

std::uint32_t SwHelper::previousPow2(std::uint32_t x) {
    if (x == 0) return 0;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

std::uint32_t SwHelper::nextPow2(std::uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

std::uint32_t SwHelper::calculateMipMapLevels(vk::Extent3D extent) { return std::floor(std::log2(std::max(extent.width, extent.height))) + 1; }
