#include <Renderer/SwLogger.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwCommandBuffer.h>
#include <Scene/SwRenderGraph.h>
#include <fmt/format.h>
#include <fmt/ostream.h>  // for fmt::print to a std::ostream
#include <quill/LogMacros.h>

#include <fstream>
#include <magic_enum.hpp>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>


SwRenderGraph::SwRenderGraph(std::vector<SwImage*> outputs) : mOutputs(std::move(outputs)) {}


void SwRenderGraph::pruneUnreachablePasses() {
    for (auto& p : mPasses) p->setPruned(true);

    // Identify passes that write each image / buffer.
    std::unordered_map<SwImage*, std::vector<SwPass*>> imageWriters;
    std::unordered_map<SwBuffer*, std::vector<SwPass*>> bufferWriters;
    for (auto& p : mPasses) {
        for (const SwDependency* deps : {&p->getStaticDeps(), &p->getDynamicDeps()}) {
            for (auto& dep : deps->mWriteImages) imageWriters[dep.mImage].emplace_back(p);
            for (auto& dep : deps->mWriteBuffers) bufferWriters[dep.mBuffer].emplace_back(p);
        }
    }

    // Map each pass to its registration index so we can distinguish earlier vs later co-writers.
    std::unordered_map<SwPass*, std::size_t> passIndex;
    passIndex.reserve(mPasses.size());
    for (std::size_t i = 0; i < mPasses.size(); ++i) passIndex[mPasses[i]] = i;

    // Setup BFS to run backwards through DAG render graph from outputs (and from must-run passes).
    std::queue<SwPass*> work;
    std::unordered_set<SwPass*> visited;

    auto visit = [&](SwPass* p) {
        if (visited.insert(p).second) work.push(p);
    };

    // BFS starts by enqueuing any pass that writes to the outputs.
    for (SwImage* out : mOutputs) {
        if (auto it = imageWriters.find(out); it != imageWriters.end()) {
            for (SwPass* writer : it->second) visit(writer);
        }
    }

    // BFS starts by enqueuing any pass which must run regardless.
    for (auto& p : mPasses) {
        if (p->isMustRun()) visit(p);
    }

    // For each enqueued pass:
    //   (a) Follow read dependencies backward to whichever passes produce those reads.
    //   (b) Follow write dependencies to pull in earlier co-writers of the same resource
    //       (W→W chain) — ensures initialization / clear passes are not mistakenly pruned
    //       even when no downstream pass explicitly reads from them.
    while (!work.empty()) {
        SwPass* p = work.front();
        work.pop();

        // (a) Read-dep backward walk
        for (const SwDependency* deps : {&p->getStaticDeps(), &p->getDynamicDeps()}) {
            for (auto& dep : deps->mReadImages) {
                if (auto it = imageWriters.find(dep.mImage); it != imageWriters.end()) {
                    for (SwPass* writer : it->second) visit(writer);
                }
            }
            for (auto& dep : deps->mReadBuffers) {
                if (auto it = bufferWriters.find(dep.mBuffer); it != bufferWriters.end()) {
                    for (SwPass* writer : it->second) visit(writer);
                }
            }

            // (b) Earlier co-writer inclusion (W→W)
            const std::size_t pIdx = passIndex.at(p);
            for (auto& dep : deps->mWriteImages) {
                if (auto it = imageWriters.find(dep.mImage); it != imageWriters.end()) {
                    for (SwPass* writer : it->second) {
                        if (passIndex.at(writer) < pIdx) visit(writer);
                    }
                }
            }
            for (auto& dep : deps->mWriteBuffers) {
                if (auto it = bufferWriters.find(dep.mBuffer); it != bufferWriters.end()) {
                    for (SwPass* writer : it->second) {
                        if (passIndex.at(writer) < pIdx) visit(writer);
                    }
                }
            }
        }
    }

    // Populate mSortedPasses in registration order (topological sort will reorder later).
    mSortedPasses.clear();
    mSortedPasses.reserve(visited.size());
    for (auto& p : mPasses) {
        if (visited.contains(p)) {
            p->setPruned(false);
            mSortedPasses.emplace_back(p);
        }
    }
}

void SwRenderGraph::sortTopological() {
    // Tie-breaker ordering: passes registered earlier get lower index.
    std::unordered_map<SwPass*, std::size_t> regIndex;
    regIndex.reserve(mSortedPasses.size());
    for (std::size_t i = 0; i < mSortedPasses.size(); ++i) regIndex[mSortedPasses[i]] = i;

    // Identify passes that read / write each image / buffer.
    std::unordered_map<SwImage*, std::vector<SwPass*>> imageWriters, imageReaders;
    std::unordered_map<SwBuffer*, std::vector<SwPass*>> bufferWriters, bufferReaders;
    for (SwPass* p : mSortedPasses) {
        for (const SwDependency* deps : {&p->getStaticDeps(), &p->getDynamicDeps()}) {
            for (auto& d : deps->mWriteImages) imageWriters[d.mImage].emplace_back(p);
            for (auto& d : deps->mReadImages) imageReaders[d.mImage].emplace_back(p);
            for (auto& d : deps->mWriteBuffers) bufferWriters[d.mBuffer].emplace_back(p);
            for (auto& d : deps->mReadBuffers) bufferReaders[d.mBuffer].emplace_back(p);
        }
    }

    // Set-based adjacency prevents duplicate edges from being counted multiple times.
    // W→R, R→W, and W→W edges can all connect the same pair of passes via the same
    // resource; storing them in a set ensures inDegree is only incremented once per
    // unique directed edge, keeping Kahn's algorithm and cycle detection accurate.
    std::unordered_map<SwPass*, std::unordered_set<SwPass*>> adj;
    std::unordered_map<SwPass*, int> inDegree;
    for (SwPass* p : mSortedPasses) inDegree[p] = 0;

    // Only add an edge when it goes in registration order (lower index → higher index).
    // For W→W and R→W this acts as a tiebreaker that matches the intended execution order.
    // For W→R, registration order is relied upon to be correct (writer registered before reader).
    auto addEdge = [&](SwPass* from, SwPass* to) {
        if (from == to) return;
        if (regIndex[from] >= regIndex[to]) return;
        if (adj[from].insert(to).second) inDegree[to]++;
    };

    // W→R: each writer must execute before each reader of the same resource.
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

    // R→W: a reader must finish before a later writer overwrites the same resource (WAR hazard).
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

    // W→W: order concurrent writers by registration index (earlier write before later write).
    // Iterate each unordered pair once (j > i) and try both directions; addEdge keeps only
    // the one going from lower to higher regIndex.
    for (auto& [img, writers] : imageWriters) {
        for (std::size_t i = 0; i < writers.size(); ++i) {
            for (std::size_t j = i + 1; j < writers.size(); ++j) {
                addEdge(writers[i], writers[j]);
                addEdge(writers[j], writers[i]);
            }
        }
    }
    for (auto& [buf, writers] : bufferWriters) {
        for (std::size_t i = 0; i < writers.size(); ++i) {
            for (std::size_t j = i + 1; j < writers.size(); ++j) {
                addEdge(writers[i], writers[j]);
                addEdge(writers[j], writers[i]);
            }
        }
    }

    // Kahn's algorithm with a min-heap priority queue for deterministic tie-breaking.
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
            if (--inDegree[next] == 0) ready.push(next);
        }
    }

    if (sorted.size() != mSortedPasses.size()) {
        std::string msg = "Cycle detected in render graph — passes stuck:";
        for (auto& [p, deg] : inDegree) {
            if (deg > 0) {
                msg += fmt::format("\n  - {} (unsatisfied incoming edges: {})", magic_enum::enum_name(p->getPassType()).data(), deg);
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
    std::unordered_set<SwImage*> allImages;
    std::unordered_set<SwBuffer*> allBuffers;
    for (auto& p : mPasses) {
        for (const SwDependency* deps : {&p->getStaticDeps(), &p->getDynamicDeps()}) {
            for (auto& d : deps->mReadImages) allImages.insert(d.mImage);
            for (auto& d : deps->mWriteImages) allImages.insert(d.mImage);
            for (auto& d : deps->mReadBuffers) allBuffers.insert(d.mBuffer);
            for (auto& d : deps->mWriteBuffers) allBuffers.insert(d.mBuffer);
        }
    }

    // Topological position of each surviving pass, for labels.
    std::unordered_map<SwPass*, size_t> sortIndex;
    for (size_t i = 0; i < mSortedPasses.size(); ++i) {
        sortIndex[mSortedPasses[i]] = i;
    }

    // Identifier helpers — Graphviz node IDs can't contain arbitrary chars,
    // so we use pointer values to guarantee uniqueness.
    auto passId = [](const SwPass* p) { return fmt::format("pass_{}", reinterpret_cast<uintptr_t>(p)); };
    auto imageId = [](const SwImage* i) { return fmt::format("img_{}", reinterpret_cast<uintptr_t>(i)); };
    auto bufferId = [](const SwBuffer* b) { return fmt::format("buf_{}", reinterpret_cast<uintptr_t>(b)); };

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
            suffix = fmt::format("\\n[{}]", sortIndex[p]);
        }
        if (p->isMustRun()) suffix += "\\nmust-run";

        fmt::print(
            out,
            "  {} [label=\"{}{}\", fillcolor=\"{}\", color=\"{}\"];\n",
            passId(p),
            magic_enum::enum_name(p->getPassType()).data(),
            suffix,
            fillColor,
            borderColor
        );
    }

    // Resources — ellipses for images, cylinders for buffers, marked if output.
    fmt::print(out, "\n  // Resources\n");
    std::unordered_set<SwImage*> outputSet(mOutputs.begin(), mOutputs.end());
    for (SwImage* img : allImages) {
        const bool isOutput = outputSet.count(img) > 0;
        const auto fill = isOutput ? "#ffd966" : "#f4f4f4";
        const auto label = isOutput ? "image\\n(output)" : "image";

        fmt::print(out, "  {} [shape=ellipse, style=filled, fillcolor=\"{}\", label=\"{}\"];\n", imageId(img), fill, label);
    }
    for (SwBuffer* buf : allBuffers) {
        fmt::print(out, "  {} [shape=cylinder, style=filled, fillcolor=\"#f4f4f4\", label=\"buffer\"];\n", bufferId(buf));
    }

    // Edges — pass → resource (writes), resource → pass (reads).
    fmt::print(out, "\n  // Reads and writes\n");
    for (auto& p : mPasses) {
        for (const SwDependency* deps : {&p->getStaticDeps(), &p->getDynamicDeps()}) {
            for (auto& d : deps->mWriteImages) {
                fmt::print(out, "  {} -> {} [color=\"#d62828\", label=\"W\"];\n", passId(p), imageId(d.mImage));
            }
            for (auto& d : deps->mReadImages) {
                fmt::print(out, "  {} -> {} [color=\"#2a9d8f\", label=\"R\"];\n", imageId(d.mImage), passId(p));
            }
            for (auto& d : deps->mWriteBuffers) {
                fmt::print(out, "  {} -> {} [color=\"#d62828\", label=\"W\"];\n", passId(p), bufferId(d.mBuffer));
            }
            for (auto& d : deps->mReadBuffers) {
                fmt::print(out, "  {} -> {} [color=\"#2a9d8f\", label=\"R\"];\n", bufferId(d.mBuffer), passId(p));
            }
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

std::string SwRenderGraph::getAllSortedPasses() const {
    static std::string out;
    out.clear();
    out.reserve(1 << 9);
    for (auto& pass : mSortedPasses) {
        out += fmt::format("{} -> ", magic_enum::enum_name(pass->getPassType()).data());
    }
    out = out.substr(0, out.size() - 4);
    return out;
}

void SwRenderGraph::compile() {
    pruneUnreachablePasses();
    sortTopological();
}

void SwRenderGraph::execute(SwCommandBuffer& commandBuffer) {
    /*exportGraphviz(fmt::format("{}/{}", LOGS_PATH, "rendergraph.dot"));
    LOG_DEBUG(SwRenderer::sRendererContext.mLogger->getQuillPtr(), "{}", getAllSortedPasses());*/

    for (SwPass* pass : mSortedPasses) {
        for (const SwDependency* deps : {&pass->getStaticDeps(), &pass->getDynamicDeps()}) {
            for (auto& dep : deps->mReadImages) {
                dep.mImage->emitTransition(commandBuffer.getRawCommandBuffer(), dep.mDesc.mStage, dep.mDesc.mAccess, dep.mDesc.mLayout);
            }
            for (auto& dep : deps->mWriteImages) {
                dep.mImage->emitTransition(commandBuffer.getRawCommandBuffer(), dep.mDesc.mStage, dep.mDesc.mAccess, dep.mDesc.mLayout);
            }
            for (auto& dep : deps->mReadBuffers) {
                dep.mBuffer->emitBarrier(commandBuffer.getRawCommandBuffer(), dep.mDesc.mStage, dep.mDesc.mAccess);
            }
            for (auto& dep : deps->mWriteBuffers) {
                dep.mBuffer->emitBarrier(commandBuffer.getRawCommandBuffer(), dep.mDesc.mStage, dep.mDesc.mAccess);
            }
        }
        pass->execute(commandBuffer.getRawCommandBuffer());
    }
    mPasses.clear();
    mOutputs.clear();
    mSortedPasses.clear();
}
