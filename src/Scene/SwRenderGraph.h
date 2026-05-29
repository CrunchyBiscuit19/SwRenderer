#pragma once

#include <Scene/SwPass.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class SwCommandBuffer;

class SwRenderGraph {
private:
    std::vector<SwPass*> mPasses;
    std::vector<SwImage*> mOutputs;
    std::vector<SwPass*> mSortedPasses; 

    void pruneUnreachablePasses();
    
    void sortTopological();

    void exportGraphviz(const std::filesystem::path& path) const;

public:
    SwRenderGraph() = default;

    SwRenderGraph(std::vector<SwImage*> outputs);

    void addPass(SwPass* pass);
    
    void compile();
    
    void execute(SwCommandBuffer& commandBuffer);
};