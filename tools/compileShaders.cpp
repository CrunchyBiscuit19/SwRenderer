#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

enum class ShaderType { Vert, Frag, Comp, Mod };

// Collects the names of functions annotated with [shader("...")]. When a file declares its own
// entry points this way it may expose several in one stage; slangc still defaults to looking for
// "main" unless every entry point is named explicitly with -entry, so we parse them out here and
// pass them all (packing them into a single SPIR-V module).
std::vector<std::string> findAnnotatedEntryPoints(const fs::path& p) {
    std::ifstream in(p);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> entries;
    const std::string tag = "[shader(";
    for (size_t pos = 0; (pos = s.find(tag, pos)) != std::string::npos;) {
        // Skip past the [shader(...)] attribute and any following attributes (e.g. [earlydepthstencil]).
        size_t cursor = s.find(']', pos);
        if (cursor == std::string::npos) break;
        ++cursor;
        while (true) {
            while (cursor < s.size() && std::isspace(static_cast<unsigned char>(s[cursor]))) ++cursor;
            if (cursor < s.size() && s[cursor] == '[') {
                size_t close = s.find(']', cursor);
                if (close == std::string::npos) { cursor = s.size(); break; }
                cursor = close + 1;
                continue;
            }
            break;
        }
        // Now positioned at "<ReturnType> <name>(": the entry-point name is the identifier just before '('.
        size_t paren = s.find('(', cursor);
        if (paren == std::string::npos) break;
        size_t end = paren;
        while (end > cursor && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
        size_t begin = end;
        while (begin > cursor && (std::isalnum(static_cast<unsigned char>(s[begin - 1])) || s[begin - 1] == '_')) --begin;
        if (begin < end) entries.push_back(s.substr(begin, end - begin));
        pos = paren;
    }
    return entries;
}

// A header module ("*.h.slang") and the names of the other modules it imports. The module name is
// the filename stem before the first dot, which is what is passed to slangc via -module-name and
// what other files reference in their `import` declarations. A file's own `module X;` declaration is
// overridden by -module-name, so the filename stem, not that declaration, is the authoritative name.
struct HeaderModule {
    fs::path mPath;
    std::string mName;
    std::vector<std::string> mImports;
};

// Pull the identifier that immediately follows a leading keyword on a comment-stripped line. For the
// line "import Foo;" with keyword "import" this returns "Foo". Returns empty when the line does not
// begin with the keyword as a whole word.
std::string identifierAfterKeyword(const std::string& line, const std::string& keyword) {
    if (line.size() <= keyword.size() || line.compare(0, keyword.size(), keyword) != 0) return "";
    if (!std::isspace(static_cast<unsigned char>(line[keyword.size()]))) return "";
    size_t i = keyword.size();
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    size_t begin = i;
    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) ++i;
    return line.substr(begin, i - begin);
}

// Scan a header module for its `import <Module>;` declarations. Line comments are stripped first so a
// commented-out import is not mistaken for a real dependency.
std::vector<std::string> findModuleImports(const fs::path& p) {
    std::ifstream in(p);
    std::vector<std::string> imports;
    std::string line;
    while (std::getline(in, line)) {
        size_t comment = line.find("//");
        if (comment != std::string::npos) line.erase(comment);
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        std::string name = identifierAfterKeyword(line.substr(start), "import");
        if (!name.empty()) imports.push_back(name);
    }
    return imports;
}

std::string shaderType(ShaderType type, bool l = false) {
    if (l) {
        switch (type) {
            case ShaderType::Vert:
                return "vertex";
            case ShaderType::Frag:
                return "fragment";
            case ShaderType::Comp:
                return "compute";
            case ShaderType::Mod:
                return "module";
        }
    } else {
        switch (type) {
            case ShaderType::Vert:
                return "vert";
            case ShaderType::Frag:
                return "frag";
            case ShaderType::Comp:
                return "comp";
            case ShaderType::Mod:
                return "mod";
        }
    }
    return "";
}

int main() {
    fs::path srcDir = fs::current_path() / "../shaders";
    fs::path outDir = fs::current_path() / "../shaders/out";
    std::string slang = "slangc";

    fs::create_directories(outDir);

    // Delete existing .spv and .slang-module files
    for (auto& p : fs::directory_iterator(outDir)) {
        auto ext = p.path().extension();
        if (ext == ".spv" || ext == ".slang-module") {
            fs::remove(p.path());
            std::string deletedPath = fs::weakly_canonical(p.path()).string();
            std::replace(deletedPath.begin(), deletedPath.end(), '\\', '/');
            std::cout << "Deleted: " << deletedPath << "\n";
        }
    }

    // Gather every header module ("*.h.slang", including those under Common) along with the modules
    // it imports. slangc resolves an `import` by reading the imported module's already-compiled
    // ".slang-module" off disk, so a module can only be built after everything it imports. Recording
    // the import edges here lets a topological sort decide a safe build order below.
    std::vector<HeaderModule> headerModules;
    std::unordered_map<std::string, int> moduleIndex;
    for (auto& f : fs::recursive_directory_iterator(srcDir)) {
        if (!f.is_regular_file()) continue;
        fs::path p = f.path();
        if (fs::relative(p, srcDir).begin()->string() == "out") continue;
        std::string filename = p.filename().string();
        if (!filename.ends_with(".h.slang")) continue;
        std::string moduleName = filename.substr(0, filename.find('.'));
        if (moduleIndex.contains(moduleName)) {
            std::cerr << "Duplicate module name '" << moduleName << "' from " << p.string() << "\n";
            return 1;
        }
        moduleIndex.emplace(moduleName, static_cast<int>(headerModules.size()));
        headerModules.push_back({p, moduleName, findModuleImports(p)});
    }

    // Depth-first post-order topological sort over the import graph. A module is appended only after
    // all of its in-graph imports, so the resulting list is already a valid compile order and header
    // modules may freely depend on one another. The visiting/visited colouring rejects an import
    // cycle, which slangc could not resolve in any order.
    std::vector<int> sortState(headerModules.size(), 0);  // 0 unvisited, 1 on stack, 2 finished
    std::vector<int> compileOrder;
    compileOrder.reserve(headerModules.size());
    bool cycleFound = false;
    std::function<void(int)> visit = [&](int u) {
        sortState[u] = 1;
        for (const auto& dep : headerModules[u].mImports) {
            auto it = moduleIndex.find(dep);
            if (it == moduleIndex.end()) continue;  // a builtin or non-header import is not ours to order
            int v = it->second;
            if (sortState[v] == 1) {
                std::cerr << "Import cycle detected involving module '" << headerModules[v].mName << "'\n";
                cycleFound = true;
                return;
            }
            if (sortState[v] == 0) visit(v);
        }
        sortState[u] = 2;
        compileOrder.push_back(u);
    };
    for (int i = 0; i < static_cast<int>(headerModules.size()) && !cycleFound; ++i) {
        if (sortState[i] == 0) visit(i);
    }
    if (cycleFound) return 1;

    // Compile the modules in dependency order.
    for (int i : compileOrder) {
        const HeaderModule& m = headerModules[i];
        std::string outName = m.mName + ".slang-module";
        fs::path outPath = outDir / outName;
        std::string cmd = std::format("{} {} -o {} -I {} -I {} -module-name {} -fvk-use-scalar-layout",
                                      slang, m.mPath.string(), outPath.string(),
                                      m.mPath.parent_path().string(), outDir.string(), m.mName);
        std::cout << "Compiling module: " << outName << "\n";
        std::system(cmd.c_str());
    }

    // Compile source files
    // Options: -stage <type> -profile sm_6_6 -target spirv -O3 -fvk-use-scalar-layout
    for (auto& f : fs::recursive_directory_iterator(srcDir)) {
        if (!f.is_regular_file()) continue;
        fs::path p = f.path();
        if (fs::relative(p, srcDir).begin()->string() == "out") continue;
        std::string filename = p.filename().string();
        ShaderType type;
        std::string stem;
        if (filename.ends_with(".vert.slang")) {
            type = ShaderType::Vert;
            stem = filename.substr(0, filename.size() - std::string_view(".vert.slang").size());
        } else if (filename.ends_with(".frag.slang")) {
            type = ShaderType::Frag;
            stem = filename.substr(0, filename.size() - std::string_view(".frag.slang").size());
        } else if (filename.ends_with(".comp.slang")) {
            type = ShaderType::Comp;
            stem = filename.substr(0, filename.size() - std::string_view(".comp.slang").size());
        } else {
            continue;
        }
        std::string outName = stem + "." + shaderType(type) + ".spv";
        fs::path outPath = outDir / outName;
        std::string cmd;
        std::vector<std::string> entryPoints = findAnnotatedEntryPoints(p);
        if (!entryPoints.empty()) {
            // Multiple [shader("...")] entry points: name each explicitly so they are all packed into one module.
            cmd = std::format("{} {} -o {} -I {} -I {} -profile sm_6_6 -target spirv -O3 -fvk-use-scalar-layout",
                              slang, p.string(), outPath.string(),
                              p.parent_path().string(), outDir.string());
            for (const auto& entryPoint : entryPoints) {
                cmd += " -entry " + entryPoint;
            }
        } else {
            cmd = std::format("{} {} -o {} -I {} -I {} -stage {} -profile sm_6_6 -target spirv -O3 -fvk-use-scalar-layout",
                              slang, p.string(), outPath.string(),
                              p.parent_path().string(), outDir.string(),
                              shaderType(type, true));
        }
        std::cout << "Compiling: " << outName << "\n";
        std::system(cmd.c_str());
    }

    return 0;
}
