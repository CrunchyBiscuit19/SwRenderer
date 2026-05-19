#pragma once

#include <Pass/SwPass.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class SwRenderGraph {
private:
    std::vector<std::unique_ptr<SwPass>> mPasses;
    std::vector<SwAllocatedImage*> mOutputs;
    std::vector<SwPass*> mSortedPasses; 

    void pruneUnreachablePasses();
    void sortTopological();

    void exportGraphviz(const std::filesystem::path& path) const;

public:
    SwRenderGraph(std::vector<std::unique_ptr<SwPass>> passes, std::vector<SwAllocatedImage*> outputs);

    void compile();
    void execute(vk::CommandBuffer cmd);
};