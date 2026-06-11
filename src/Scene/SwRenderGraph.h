#pragma once

#include <Scene/SwPass.h>
#include <Renderer/SwRendererContext.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class SwCommandBuffer;

class SwRenderGraph {
private:
    static const std::filesystem::path RENDER_GRAPH_EXPORT_PATH; 

    std::optional<std::ofstream> mExportStream{std::nullopt};

    std::vector<SwPass*> mPasses;
    std::vector<SwImage*> mOutputs;
    std::vector<SwPass*> mSortedPasses; 

    void pruneUnreachablePasses();
    void sortTopological();

public:
    SwRenderGraph() = default;

    SwRenderGraph(std::vector<SwImage*> outputs);

    static void init();

    void addPass(SwPass* pass) { mPasses.emplace_back(pass); }
    void addOutput(SwImage* output) { mOutputs.emplace_back(output); }
    void compile();
    void execute(SwCommandBuffer& commandBuffer);

    void requestRenderGraph(std::filesystem::path path = RENDER_GRAPH_EXPORT_PATH);
    void exportRenderGraph();
};