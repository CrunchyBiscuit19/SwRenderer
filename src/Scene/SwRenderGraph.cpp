#include <Scene/SwRenderGraph.h>
#include <fmt/format.h>
#include <fmt/ostream.h>  // for fmt::print to a std::ostream

#include <fstream>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

SwRenderGraph::SwRenderGraph(std::vector<SwAllocatedImage*> outputs)
    : mOutputs(std::move(outputs)) {}

void SwRenderGraph::pruneUnreachablePasses() {
    for (auto& p : mPasses) p->setPruned(true);

    // Identify passes that write each image / buffer.
    std::unordered_map<SwAllocatedImage*, std::vector<SwPass*>> imageWriters;
    std::unordered_map<SwAllocatedBuffer*, std::vector<SwPass*>> bufferWriters;
    for (auto& p : mPasses) {
        for (auto& dep : p->getWriteImages()) imageWriters[dep.mImage].emplace_back(p);
        for (auto& dep : p->getWriteBuffers()) bufferWriters[dep.mBuffer].emplace_back(p);
    }

    // Setup BFS to run backwards through DAG render graph from outputs (and from must-run passes).
    std::queue<SwPass*> work;
    std::unordered_set<SwPass*> visited;

    auto visit = [&](SwPass* p) {
        if (visited.insert(p).second) work.push(p);
    };

    // BFS starts by enqueuing any pass that writes to the outputs.
    for (SwAllocatedImage* out : mOutputs) {
        if (auto it = imageWriters.find(out); it != imageWriters.end()) {
            for (SwPass* writer : it->second) visit(writer);
        }
    }

    // BFS starts by enqueuing any pass which must run regardless.
    for (auto& p : mPasses) {
        if (p->isMustRun()) visit(p);
    }

    // For each enqueued pass, follow its read dependencies back to whichever passes produce those reads.
    while (!work.empty()) {
        SwPass* p = work.front();
        work.pop();
        for (auto& dep : p->getReadImages()) {
            if (auto it = imageWriters.find(dep.mImage); it != imageWriters.end()) {
                for (SwPass* writer : it->second) visit(writer);
            }
        }
        for (auto& dep : p->getReadBuffers()) {
            if (auto it = bufferWriters.find(dep.mBuffer); it != bufferWriters.end()) {
                for (SwPass* writer : it->second) visit(writer);
            }
        }
    }

    // Setup for topological sort later
    mSortedPasses.clear();
    mSortedPasses.reserve(visited.size());
    for (auto& p : mPasses) {
        if (visited.count(p) >= 1) {
            p->setPruned(false);
            mSortedPasses.emplace_back(p);
        }
    }
}

void SwRenderGraph::sortTopological() {
    // Tie-breaker ordering
    std::unordered_map<SwPass*, size_t> regIndex;
    for (size_t i = 0; i < mSortedPasses.size(); ++i) {
        regIndex[mSortedPasses[i]] = i;
    }

    // Identify passes that read / write each image / buffer.    }
    std::unordered_map<SwAllocatedImage*, std::vector<SwPass*>> imageWriters, imageReaders;
    std::unordered_map<SwAllocatedBuffer*, std::vector<SwPass*>> bufferWriters, bufferReaders;
    for (SwPass* p : mSortedPasses) {
        for (auto& d : p->getWriteImages()) imageWriters[d.mImage].emplace_back(p);
        for (auto& d : p->getReadImages()) imageReaders[d.mImage].emplace_back(p);
        for (auto& d : p->getWriteBuffers()) bufferWriters[d.mBuffer].emplace_back(p);
        for (auto& d : p->getReadBuffers()) bufferReaders[d.mBuffer].emplace_back(p);
    }

    // Adjacency list / in-degree count of dependencies.
    std::unordered_map<SwPass*, std::vector<SwPass*>> adj;
    std::unordered_map<SwPass*, int> inDegree;
    for (SwPass* p : mSortedPasses) inDegree[p] = 0;

    auto addEdge = [&](SwPass* from, SwPass* to) {
        if (from == to) return;
        if (regIndex[from] >= regIndex[to]) return;
        adj[from].emplace_back(to);
        inDegree[to]++;
    };

    // W->R: writer → reader
    for (auto& [img, writers] : imageWriters) {
        for (SwPass* w : writers) {
            for (SwPass* r : imageReaders[img]) addEdge(w, r);
        }
    }
    for (auto& [buf, writers] : bufferWriters) {
        for (SwPass* w : writers) {
            for (SwPass* r : bufferReaders[buf]) addEdge(w, r);
        }
    }

    // R->W: earlier reader → later writer
    for (auto& [img, readers] : imageReaders) {
        for (SwPass* r : readers) {
            for (SwPass* w : imageWriters[img]) addEdge(r, w);
        }
    }
    for (auto& [buf, readers] : bufferReaders) {
        for (SwPass* r : readers) {
            for (SwPass* w : bufferWriters[buf]) addEdge(r, w);
        }
    }

    // W->W: earlier writer → later writer
    for (auto& [img, writers] : imageWriters) {
        for (size_t i = 0; i < writers.size(); ++i) {
            for (size_t j = 0; j < writers.size(); ++j) {
                if (i != j) addEdge(writers[i], writers[j]);
            }
        }
    }
    for (auto& [buf, writers] : bufferWriters) {
        for (size_t i = 0; i < writers.size(); ++i) {
            for (size_t j = 0; j < writers.size(); ++j) {
                if (i != j) addEdge(writers[i], writers[j]);
            }
        }
    }

    // Do the sorting part with priority queue (min heap based on regIndex) to get a deterministic order.
    auto cmp = [&](SwPass* a, SwPass* b) { return regIndex[a] > regIndex[b]; };
    std::priority_queue<SwPass*, std::vector<SwPass*>, decltype(cmp)> ready(cmp);
    for (auto& [p, deg] : inDegree) {
        if (deg == 0) ready.push(p);
    }

    std::vector<SwPass*> sorted;
    sorted.reserve(mSortedPasses.size());
    while (!ready.empty()) {
        SwPass* p = ready.top();
        ready.pop();
        sorted.emplace_back(p);
        for (SwPass* next : adj[p]) {
            if (--inDegree.at(next) == 0) ready.push(next);
        }
    }

    if (sorted.size() != mSortedPasses.size()) {
        std::string msg = "Cycle detected in render graph among passes:";
        for (auto& [p, deg] : inDegree) {
            if (deg > 0) {
                msg += fmt::format("\n  - {} ({} dependencies)", p->getName(), deg);
            }
        }
        throw std::runtime_error(msg);
    }

    mSortedPasses = std::move(sorted);
}

void SwRenderGraph::exportGraphviz(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error(fmt::format("Failed to open Graphviz output file: {}", path.string()));
    }

    // Collect every resource referenced by any pass (pruned or not), so the
    // graph shows the full picture and you can see what got culled.
    std::unordered_set<SwAllocatedImage*> allImages;
    std::unordered_set<SwAllocatedBuffer*> allBuffers;
    for (auto& p : mPasses) {
        for (auto& d : p->getReadImages()) allImages.insert(d.mImage);
        for (auto& d : p->getWriteImages()) allImages.insert(d.mImage);
        for (auto& d : p->getReadBuffers()) allBuffers.insert(d.mBuffer);
        for (auto& d : p->getWriteBuffers()) allBuffers.insert(d.mBuffer);
    }

    // Topological position of each surviving pass, for labels.
    std::unordered_map<SwPass*, size_t> sortIndex;
    for (size_t i = 0; i < mSortedPasses.size(); ++i) {
        sortIndex[mSortedPasses[i]] = i;
    }

    // Identifier helpers — Graphviz node IDs can't contain arbitrary chars,
    // so we use pointer values to guarantee uniqueness.
    auto passId = [](const SwPass* p) { return fmt::format("pass_{}", reinterpret_cast<uintptr_t>(p)); };
    auto imageId = [](const SwAllocatedImage* i) { return fmt::format("img_{}", reinterpret_cast<uintptr_t>(i)); };
    auto bufferId = [](const SwAllocatedBuffer* b) { return fmt::format("buf_{}", reinterpret_cast<uintptr_t>(b)); };

    fmt::print(out, "digraph RenderGraph {{\n");
    fmt::print(out, "  rankdir=LR;\n");
    fmt::print(out, "  node [fontname=\"Helvetica\"];\n");
    fmt::print(out, "  edge [fontname=\"Helvetica\", fontsize=10];\n\n");

    // Passes — box nodes, colored by status.
    fmt::print(out, "  // Passes\n");
    fmt::print(out, "  node [shape=box, style=\"rounded,filled\"];\n");
    for (auto& p : mPasses) {
        const bool pruned = p->isPruned();
        const auto fillColor = pruned ? "#eeeeee" : "#cce5ff";
        const auto borderColor = pruned ? "#999999" : "#000000";

        std::string suffix;
        if (pruned) {
            suffix = "\\n(pruned)";
        } else {
            suffix = fmt::format("\\n[{}]", sortIndex.at(p));
        }
        if (p->isMustRun()) suffix += "\\nmust-run";

        fmt::print(out, "  {} [label=\"{}{}\", fillcolor=\"{}\", color=\"{}\"];\n", passId(p), p->getName(), suffix, fillColor, borderColor);
    }

    // Resources — ellipses for images, cylinders for buffers, marked if output.
    fmt::print(out, "\n  // Resources\n");
    std::unordered_set<SwAllocatedImage*> outputSet(mOutputs.begin(), mOutputs.end());
    for (SwAllocatedImage* img : allImages) {
        const bool isOutput = outputSet.count(img) > 0;
        const auto fill = isOutput ? "#ffd966" : "#f4f4f4";
        const auto label = isOutput ? "image\\n(output)" : "image";

        fmt::print(out, "  {} [shape=ellipse, style=filled, fillcolor=\"{}\", label=\"{}\"];\n", imageId(img), fill, label);
    }
    for (SwAllocatedBuffer* buf : allBuffers) {
        fmt::print(out, "  {} [shape=cylinder, style=filled, fillcolor=\"#f4f4f4\", label=\"buffer\"];\n", bufferId(buf));
    }

    // Edges — pass → resource (writes), resource → pass (reads).
    fmt::print(out, "\n  // Reads and writes\n");
    for (auto& p : mPasses) {
        for (auto& d : p->getWriteImages()) {
            fmt::print(out, "  {} -> {} [color=\"#d62828\", label=\"W\"];\n", passId(p), imageId(d.mImage));
        }
        for (auto& d : p->getReadImages()) {
            fmt::print(out, "  {} -> {} [color=\"#2a9d8f\", label=\"R\"];\n", imageId(d.mImage), passId(p));
        }
        for (auto& d : p->getWriteBuffers()) {
            fmt::print(out, "  {} -> {} [color=\"#d62828\", label=\"W\"];\n", passId(p), bufferId(d.mBuffer));
        }
        for (auto& d : p->getReadBuffers()) {
            fmt::print(out, "  {} -> {} [color=\"#2a9d8f\", label=\"R\"];\n", bufferId(d.mBuffer), passId(p));
        }
    }

    // Execution order — dashed edges between consecutive sorted passes, so
    // the topological order is visually obvious independent of the DAG layout.
    if (mSortedPasses.size() >= 2) {
        fmt::print(out, "\n  // Execution order\n");
        for (size_t i = 0; i + 1 < mSortedPasses.size(); ++i) {
            fmt::print(
                out, "  {} -> {} [style=dashed, color=\"#888888\", constraint=false, label=\"next\"];\n", passId(mSortedPasses[i]), passId(mSortedPasses[i + 1])
            );
        }
    }

    fmt::print(out, "}}\n");
}

void SwRenderGraph::addPass(SwPass* pass) { mPasses.emplace_back(pass); }

void SwRenderGraph::compile() {
    pruneUnreachablePasses();
    sortTopological();
}

void SwRenderGraph::execute(vk::CommandBuffer cmd) {
    for (SwPass* pass : mSortedPasses) {
        for (auto& dep : pass->getReadImages()) {
            dep.mImage->emitTransition(cmd, dep.mLayout, dep.mStage, dep.mAccess);
        }
        for (auto& dep : pass->getWriteImages()) {
            dep.mImage->emitTransition(cmd, dep.mLayout, dep.mStage, dep.mAccess);
        }
        for (auto& dep : pass->getReadBuffers()) {
            dep.mBuffer->emitBarrier(cmd, dep.mStage, dep.mAccess);
        }
        for (auto& dep : pass->getWriteBuffers()) {
            dep.mBuffer->emitBarrier(cmd, dep.mStage, dep.mAccess);
        }

        pass->execute(cmd);
    }
}