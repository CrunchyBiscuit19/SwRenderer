#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum class ShaderType { Vert, Frag, Comp, Mod };

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

    // Compile common modules
    fs::path commonDir = srcDir / "Common";
    for (auto& f : fs::directory_iterator(commonDir)) {
        fs::path fullPath = f.path();
        std::string filename = fullPath.filename().string();
        if (!filename.ends_with(".h.slang")) continue;
        std::string moduleName = filename.substr(0, filename.find('.'));
        std::string outName = moduleName + ".slang-module";
        fs::path outPath = outDir / outName;
        std::string cmd = std::format("{} {} -o {} -I {} -module-name {} -fvk-use-scalar-layout",
                                      slang, fullPath.string(), outPath.string(),
                                      commonDir.string(), moduleName);
        std::cout << "Compiling module: " << outName << "\n";
        std::system(cmd.c_str());
    }

    // Compile header modules
    for (auto& f : fs::recursive_directory_iterator(srcDir)) {
        if (!f.is_regular_file()) continue;
        fs::path p = f.path();
        if (fs::equivalent(p.parent_path(), commonDir)) continue;
        if (fs::relative(p, srcDir).begin()->string() == "out") continue;
        std::string filename = p.filename().string();
        if (!filename.ends_with(".h.slang")) continue;
        std::string moduleName = filename.substr(0, filename.find('.'));
        std::string outName = moduleName + ".slang-module";
        fs::path outPath = outDir / outName;
        std::string cmd = std::format("{} {} -o {} -I {} -I {} -module-name {} -fvk-use-scalar-layout",
                                      slang, p.string(), outPath.string(),
                                      p.parent_path().string(), outDir.string(), moduleName);
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
        std::string cmd = std::format("{} {} -o {} -I {} -I {} -stage {} -profile sm_6_6 -target spirv -O3 -fvk-use-scalar-layout",
                                      slang, p.string(), outPath.string(),
                                      p.parent_path().string(), outDir.string(),
                                      shaderType(type, true));
        std::cout << "Compiling: " << outName << "\n";
        std::system(cmd.c_str());
    }

    return 0;
}
