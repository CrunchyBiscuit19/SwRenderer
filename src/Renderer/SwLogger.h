#pragma once

#include <quill/Logger.h>

#include <cstdint>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

#if defined(_MSC_VER)
    #define SW_DEBUG_BREAK() __debugbreak()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
    #define SW_DEBUG_BREAK() __builtin_debugtrap()
#else
    #include <csignal>
    #define SW_DEBUG_BREAK() raise(SIGTRAP)
#endif

class SwLogger {
public:
    enum class LogLocation { Console, File, Both };
    static constexpr LogLocation LOG_LOCATION{LogLocation::Both};
    static constexpr quill::LogLevel LOG_LEVEL{quill::LogLevel::Debug};

    SwLogger();

    inline quill::Logger* getQuillPtr() { return mLogger; }
    inline std::uint64_t getFrameNumber() const { return mFrameNumber ? *mFrameNumber : 0; }
    inline void setFrameNumber(const std::uint64_t* ptr) { mFrameNumber = ptr; }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
    );

private:
    quill::Logger* mLogger{nullptr};
    const std::uint64_t* mFrameNumber{nullptr};
    std::unordered_set<std::string> mBlockedMessages;
    std::unordered_set<std::string> mBreakMessages;
};
