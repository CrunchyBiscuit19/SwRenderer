# CMakeList.txt : CMake project for SwRenderer, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 4.3.2)

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Not sure what this is
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")

# Change output directory of binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Project declaration
project(SwRenderer)

# ---------------------------------------------------------------------------
# Shaders
# ---------------------------------------------------------------------------
set(CMAKE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

find_program(SLANGC slangc REQUIRED)

file(GLOB_RECURSE SLANG_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SHADER_DIR}/*.slang"
	"${CMAKE_SHADER_DIR}/**/*.slang"
	"${CMAKE_SHADER_DIR}/slangdconfig.json"
)

# Entry-point shaders compile to a .spv next to their source. Imported modules are compiled from
# source as part of each shader, and the slangc depfile records those imports so that a module edit
# rebuilds exactly the .spv files that transitively depend on it.
file(GLOB_RECURSE SLANG_ENTRY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SHADER_DIR}/**/*.vert.slang"
    "${CMAKE_SHADER_DIR}/**/*.frag.slang"
    "${CMAKE_SHADER_DIR}/**/*.comp.slang"
)

# slangd drops precompiled *.slang-module binaries next to module sources for IDE intellisense. slangc
# prefers a precompiled module over its .slang source when both are on the include path and does not
# check whether the source is newer, so a stale module silently shadows an edited one (a constant added
# to SwConstants.slang goes unseen). Purge them before every shader build so slangc reads source.
set(SLANG_MODULE_PURGE_SCRIPT "${CMAKE_BINARY_DIR}/purgeSlangModules.cmake")
file(WRITE "${SLANG_MODULE_PURGE_SCRIPT}"
    "file(GLOB_RECURSE _slangModules \"${CMAKE_SHADER_DIR}/*.slang-module\")\n"
    "if(_slangModules)\n"
    "    file(REMOVE \${_slangModules})\n"
    "endif()\n"
)
add_custom_target(PurgeSlangModules
    COMMAND ${CMAKE_COMMAND} -P "${SLANG_MODULE_PURGE_SCRIPT}"
    COMMENT "Purging stale Slang precompiled modules"
    VERBATIM
)

set(SPV_OUTPUTS "")
foreach(SHADER IN LISTS SLANG_ENTRY_SOURCES)
    if(SHADER MATCHES "\\.vert\\.slang$")
        set(SHADER_STAGE vertex)
    elseif(SHADER MATCHES "\\.frag\\.slang$")
        set(SHADER_STAGE fragment)
    else()
        set(SHADER_STAGE compute)
    endif()

    string(REGEX REPLACE "\\.slang$" ".spv" SHADER_SPV "${SHADER}")
    file(RELATIVE_PATH SHADER_REL "${CMAKE_SHADER_DIR}" "${SHADER}")
    set(SHADER_DEPFILE "${CMAKE_BINARY_DIR}/shaderDeps/${SHADER_REL}.d")
    get_filename_component(SHADER_DEPFILE_DIR "${SHADER_DEPFILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${SHADER_DEPFILE_DIR}")

    # Files that declare [shader("...")] entry points name each one explicitly so slangc packs them
    # into a single module, while the rest compile their implicit "main" for the filename's stage.
    file(READ "${SHADER}" SHADER_TEXT)
    string(REGEX MATCHALL "\\[shader\\(\"[a-z]+\"\\)\\][^(]*\\(" SHADER_ENTRY_DECLS "${SHADER_TEXT}")
    set(SHADER_STAGE_ARGS "")
    if(SHADER_ENTRY_DECLS)
        foreach(ENTRY_DECL IN LISTS SHADER_ENTRY_DECLS)
            string(REGEX MATCH "([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*\\($" ENTRY_MATCH "${ENTRY_DECL}")
            list(APPEND SHADER_STAGE_ARGS -entry "${CMAKE_MATCH_1}")
        endforeach()
    else()
        set(SHADER_STAGE_ARGS -stage ${SHADER_STAGE})
    endif()

    add_custom_command(
        OUTPUT "${SHADER_SPV}"
        COMMAND ${SLANGC} "${SHADER}" -o "${SHADER_SPV}" -I "${CMAKE_SHADER_DIR}"
                ${SHADER_STAGE_ARGS} -profile sm_6_6 -target spirv -O3 -fvk-use-scalar-layout
                -depfile "${SHADER_DEPFILE}"
        DEPFILE "${SHADER_DEPFILE}"
        DEPENDS "${SHADER}" PurgeSlangModules
        COMMENT "Compiling shader ${SHADER_REL}"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS "${SHADER_SPV}")
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_OUTPUTS} SOURCES ${SLANG_SOURCES})

source_group(TREE "${CMAKE_SHADER_DIR}"
             PREFIX "Shaders"
             FILES ${SLANG_SOURCES})

# ---------------------------------------------------------------------------
# Documentation (.md files)
# ---------------------------------------------------------------------------
# Root-level .md only (non-recursive) so thirdParty READMEs are not swept in,
# plus everything under docs/.
file(GLOB MARKDOWN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.md"
)
file(GLOB_RECURSE MARKDOWN_DOCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/*.md"
)
list(APPEND MARKDOWN_SOURCES ${MARKDOWN_DOCS})

# These are attached to the SwRenderer target below (not a standalone custom
# target) so Visual Studio's CMake Targets View actually surfaces them.

# ---------------------------------------------------------------------------
# Main executable sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
)
file(GLOB_RECURSE HEADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h"
)

add_executable(SwRenderer ${CPP_SOURCES} ${HEADER_SOURCES} ${MARKDOWN_SOURCES})
set_property(TARGET SwRenderer PROPERTY CXX_STANDARD 23)

# .md files are documentation only - never hand them to the compiler.
set_source_files_properties(${MARKDOWN_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Mirror on-disk folder structure in IDE solution explorer,
# with headers and sources mixed together by folder, all under "Source Files"
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files"
             FILES ${CPP_SOURCES} ${HEADER_SOURCES})

# Documentation files grouped under their own "Docs" filter.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
             PREFIX "Docs"
             FILES ${MARKDOWN_SOURCES})

# Set SwRenderer as the default startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SwRenderer)

# ---------------------------------------------------------------------------
# Vulkan
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# ---------------------------------------------------------------------------
# Third-party libraries with their own CMakeLists
# ---------------------------------------------------------------------------
add_subdirectory(thirdParty/fastgltf)

# Build SDL3 as a DLL (+ import lib) only: skip the static lib, test lib, tests,
# examples and install rules. Must be set before add_subdirectory.
set(SDL_SHARED       ON  CACHE BOOL "" FORCE)
set(SDL_STATIC       OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdParty/SDL3-3.4.10)

# ---------------------------------------------------------------------------
# Third-party libraries built from source here
# ---------------------------------------------------------------------------
set(CMAKE_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty")

# imgui
add_library(imgui STATIC)
target_include_directories(imgui PUBLIC "${CMAKE_THIRD_PARTY_DIR}/imgui")
target_sources(imgui PRIVATE
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_demo.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_impl_vulkan.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_impl_sdl3.cpp"
    "${CMAKE_THIRD_PARTY_DIR}/imgui/ImGuizmo.cpp"
    #"${CMAKE_THIRD_PARTY_DIR}/imgui/GraphEditor.cpp"
    #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImCurveEdit.cpp"
    #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImGradient.cpp"
    #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImSequencer.cpp"
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL3::SDL3)

# vk-bootstrap
add_library(vkbootstrap STATIC)
target_include_directories(vkbootstrap PUBLIC "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap")
target_sources(vkbootstrap PRIVATE
    "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap/VkBootstrap.h"
    "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap/VkBootstrap.cpp"
)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 23)

# ---------------------------------------------------------------------------
# Header-only third-party libraries
# ---------------------------------------------------------------------------
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${CMAKE_THIRD_PARTY_DIR}/glm")

add_library(magic_enum INTERFACE)
target_include_directories(magic_enum INTERFACE "${CMAKE_THIRD_PARTY_DIR}/magic_enum")

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_THIRD_PARTY_DIR}/stb_image")

# tinyexr (+ bundled miniz) for OpenEXR loading. miniz.c is a real TU and tinyexr_impl.cpp pulls in
# the tinyexr implementation, so this is a compiled static library rather than header-only.
add_library(tinyexr STATIC
    "${CMAKE_THIRD_PARTY_DIR}/tinyexr/miniz.c"
    "${CMAKE_THIRD_PARTY_DIR}/tinyexr/tinyexr_impl.cpp"
)
target_include_directories(tinyexr PUBLIC "${CMAKE_THIRD_PARTY_DIR}/tinyexr")

add_library(imgui-filebrowser INTERFACE)
target_include_directories(imgui-filebrowser INTERFACE "${CMAKE_THIRD_PARTY_DIR}/imgui-filebrowser")

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE "${CMAKE_THIRD_PARTY_DIR}/vma")

add_library(quill INTERFACE)
target_include_directories(quill INTERFACE "${CMAKE_THIRD_PARTY_DIR}")

# Group all third-party targets under a single "ThirdParty" folder in the IDE
set_target_properties(imgui vkbootstrap tinyexr PROPERTIES FOLDER "ThirdParty")

# ---------------------------------------------------------------------------
# Includes, links, dependencies
# ---------------------------------------------------------------------------
target_include_directories(SwRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/")
target_include_directories(SwRenderer PUBLIC "${CMAKE_THIRD_PARTY_DIR}/SDL3-3.4.10/include")
target_link_libraries(SwRenderer PUBLIC
    vkbootstrap
    imgui
    glm
    magic_enum
    stb_image
    tinyexr
    imgui-filebrowser
    vma
    quill
    Vulkan::Vulkan
    SDL3::SDL3
    fastgltf::fastgltf
)
add_dependencies(SwRenderer Shaders)

# ---------------------------------------------------------------------------
# Precompiled headers and compile definitions
# ---------------------------------------------------------------------------
target_precompile_headers(SwRenderer PUBLIC
    <optional>
    <vector>
    <memory>
    <string>
    <unordered_map>
    <glm/mat4x4.hpp>
    <glm/vec4.hpp>
    <vulkan/vulkan.hpp>
    <vulkan/vulkan_raii.hpp>
)

target_compile_definitions(SwRenderer PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(SwRenderer PUBLIC LOGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/logs/")
target_compile_definitions(SwRenderer PUBLIC ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/")
target_compile_definitions(SwRenderer PUBLIC LIGHTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/lights/")
target_compile_definitions(SwRenderer PUBLIC SKYBOXES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/skyboxes/")
target_compile_definitions(SwRenderer PUBLIC DOCS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/docs")

target_compile_definitions(SwRenderer PUBLIC SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/System")

target_compile_definitions(SwRenderer PRIVATE $<$<CONFIG:Release>:RELEASE>)

# Vulkan debug object labels (VK_EXT_debug_utils naming): enabled for Debug and RelWithDebInfo,
# compiled out of Release. NDEBUG can't distinguish RelWithDebInfo from Release, so gate it here.
target_compile_definitions(SwRenderer PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:SW_ENABLE_DEBUG_LABELS=1>
)

# ---------------------------------------------------------------------------
# Post-build: copy runtime DLLs (e.g. SDL) next to the executable
# ---------------------------------------------------------------------------
add_custom_command(TARGET SwRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:SwRenderer> $<TARGET_RUNTIME_DLLS:SwRenderer>
    COMMAND_EXPAND_LISTS
)