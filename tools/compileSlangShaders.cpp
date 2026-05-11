#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <string>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

enum class ShaderType {
    Vert,
    Frag,
    Comp,
    Mod
};

std::string shaderType(ShaderType type, bool l = false) {
    if (l) {
        switch (type) {
            case ShaderType::Vert: return "vertex";
            case ShaderType::Frag: return "fragment";
            case ShaderType::Comp: return "compute";
            case ShaderType::Mod:  return "module";
        }
    } else {
        switch (type) {
            case ShaderType::Vert: return "vert";
            case ShaderType::Frag: return "frag";
            case ShaderType::Comp: return "comp";
            case ShaderType::Mod:  return "mod";
        }
    }
    return "";
}

struct ShaderFile {
    fs::path fullName;
    std::string shortName;
    ShaderType type;

    ShaderFile(fs::path _fullName, std::string _shortName, ShaderType _type): fullName(_fullName), shortName(_shortName), type(_type) {}
};

int main() {
    fs::path script_path = fs::current_path(); // Assuming executable runs in script dir
    fs::path shaders_source_dir = script_path / "../shaders/source";
    fs::path shaders_out_dir = script_path / "../shaders/out";
    std::string slang_compiler = "slangc";

    fs::create_directories(shaders_out_dir);

    // Delete existing .spv and .slang-module files
    for (auto& p : fs::directory_iterator(shaders_out_dir)) {
        auto ext = p.path().extension();
        if (ext == ".spv" || ext == ".slang-module") {
            fs::remove(p.path());
            std::string deletedPath = std::filesystem::weakly_canonical(p.path()).string();
            std::replace(deletedPath.begin(), deletedPath.end(), '\\', '/');
            std::cout << "Deleted: " << deletedPath << "\n";
        }
    }

    // Common modules set
    std::unordered_set<fs::path> common = {
        shaders_source_dir / "Instance.slang",
        shaders_source_dir / "MaterialConstant.slang",
        shaders_source_dir / "NodeTransform.slang",
        shaders_source_dir / "Perspective.slang",
        shaders_source_dir / "RenderItem.slang",
        shaders_source_dir / "Vertex.slang"
    };

    std::vector<ShaderFile> commonModules;
    std::vector<ShaderFile> otherModules;
    std::vector<ShaderFile> topLevel;

    for (auto& p : fs::directory_iterator(shaders_source_dir)) {
        if (p.path().extension() != ".slang")
            continue;

        std::string filename = p.path().filename().string();
        std::string shader_name;
        ShaderType shader_type;

        size_t first_dot = filename.find('.');
        size_t last_dot = filename.rfind('.');

        if (first_dot != last_dot) {
            shader_name = filename.substr(0, first_dot);
            std::string s = filename.substr(first_dot + 1, last_dot - first_dot - 1);
            if (s == "vert") shader_type = ShaderType::Vert;
            if (s == "frag") shader_type = ShaderType::Frag;
            if (s == "comp") shader_type = ShaderType::Comp;
        } else {
            shader_name = filename.substr(0, last_dot);
            shader_type = ShaderType::Mod;
        }

        if (shader_type == ShaderType::Mod) {
            if (common.contains(p.path())) {
                commonModules.emplace_back(p.path(), shader_name, shader_type);
            } else {
                otherModules.emplace_back(p.path(), shader_name, shader_type);
            }
        } else {
            topLevel.emplace_back(p.path(), shader_name, shader_type);
        }
    }

    for (auto& sf: commonModules) {
        fs::path output_file = shaders_out_dir / (sf.shortName + ".slang-module");
        std::string cmd = slang_compiler + " " + sf.fullName.string() + " -o " + output_file.string() +
                " -I " + shaders_source_dir.string() +
                " -module-name " + sf.shortName + " -fvk-use-scalar-layout";
        std::system(cmd.c_str());
        std::cout << sf.shortName << ".slang-module\n";
    }

    for (auto& sf: otherModules) {
        fs::path output_file = shaders_out_dir / (sf.shortName + ".slang-module");
        std::string cmd = slang_compiler + " " + sf.fullName.string() + " -o " + output_file.string() +
                " -I " + shaders_source_dir.string() +
                " -module-name " + sf.shortName + " -fvk-use-scalar-layout";
        std::system(cmd.c_str());
        std::cout << sf.shortName << ".slang-module\n";
    }

    for (auto& sf: topLevel) {
        fs::path output_file = shaders_out_dir / (sf.shortName + "." + shaderType(sf.type) + ".spv");
        std::string cmd = slang_compiler + " " + sf.fullName.string() + " -o " + output_file.string() +
                " -I " + shaders_source_dir.string() +
                " -stage " + shaderType(sf.type, true) +
                " -profile sm_6_6 -target spirv -O3" +
                " -fvk-use-scalar-layout";
        std::system(cmd.c_str());
        std::cout << sf.shortName << "." << shaderType(sf.type) << ".spv\n";
    }

    return 0;
}