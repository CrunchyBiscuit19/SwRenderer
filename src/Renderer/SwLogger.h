#pragma once

#include <quill/Logger.h>

#include <cstdint>
#include <unordered_set>
#include <unordered_map>
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
    static const std::filesystem::path RECORDED_MESSAGES_PATH;

    enum class LogLocation { Console, File, Both };
    static constexpr LogLocation LOG_LOCATION{LogLocation::Both};
    static constexpr quill::LogLevel LOG_LEVEL{quill::LogLevel::Warning};

    SwLogger();

    inline quill::Logger* getQuillPtr() { return mLogger; }
    inline std::uint64_t getFrameNumber() const { return mFrameNumber ? *mFrameNumber : 0; }
    inline void setFrameNumber(const std::uint64_t* ptr) { mFrameNumber = ptr; }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
    );

    void writeRecordedMessages();

private:
    quill::Logger* mLogger{nullptr};
    const std::uint64_t* mFrameNumber{nullptr};
    std::unordered_set<std::string> mBlockedMessages;
    std::unordered_set<std::string> mBreakMessages;
    std::unordered_map<std::string, std::unordered_set<std::uint64_t>> mMessageRecords;
};
