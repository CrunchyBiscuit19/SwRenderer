#!/usr/bin/env python3
"""Detect and optionally remove unused #include / import lines under src/ and shaders/.

Method: mutation testing against the real compiler, not heuristic symbol matching.
For each include/import line in a file, comment it out, recompile, and keep the
line removed only if the recompile still succeeds. This is the only reliable way
to prove an include is unused, since header-name-to-symbol heuristics produce
false positives/negatives constantly in a codebase this size.

Shader mode (--shaders) is fully reliable: slangc is invoked directly with the
exact flags CMakeLists.txt uses, per shader, so every removal is verified against
the real compiler.

C++ mode (--cpp) is best-effort only. It shells out to cl.exe with include paths
scraped from CMakeLists.txt plus a forced include of SwPch.h, but it cannot
perfectly reproduce the MSVC/CMake build (fastgltf add_subdirectory include
dirs, config-specific defines, etc). Always review the diff and let the project
rebuild afterward rather than trusting this blindly.

Usage:
    python tools/remove_unused_includes.py --shaders                # dry run, report only
    python tools/remove_unused_includes.py --shaders --apply         # apply removals
    python tools/remove_unused_includes.py --cpp --apply             # best-effort C++ pass
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHADERS_DIR = ROOT / "shaders"
SRC_DIR = ROOT / "src"

IMPORT_RE = re.compile(r"^\s*import\s+[\w.]+\s*;\s*$")
INCLUDE_RE = re.compile(r"^\s*#include\s*[<\"][^>\"]+[>\"]\s*$")
ENTRY_ATTR_RE = re.compile(r'\[shader\("([a-z]+)"\)\][^(]*\(')


def find_slangc() -> str:
    slangc = shutil.which("slangc")
    if slangc:
        return slangc
    fallback = Path("C:/VulkanSDK/1.4.335.0/Bin/slangc.exe")
    if fallback.exists():
        return str(fallback)
    sys.exit("slangc not found on PATH and no fallback VulkanSDK install located.")


def find_cl() -> str:
    cl = shutil.which("cl")
    if not cl:
        sys.exit(
            "cl.exe not found on PATH. Run this from an 'x64 Native Tools Command "
            "Prompt for VS' (or a shell where vcvarsall.bat has been run)."
        )
    return cl


def entry_args(shader_path: Path, text: str) -> list[str]:
    if ENTRY_ATTR_RE.search(text):
        args = []
        for m in re.finditer(r'\[shader\("[a-z]+"\)\]\s*(?:\w+\s+)*?(\w+)\s*\(', text):
            args += ["-entry", m.group(1)]
        return args
    name = shader_path.name
    if name.endswith(".vert.slang"):
        return ["-stage", "vertex"]
    if name.endswith(".frag.slang"):
        return ["-stage", "fragment"]
    if name.endswith(".comp.slang"):
        return ["-stage", "compute"]
    return []


def compile_shader(slangc: str, entry_file: Path, out_dir: Path) -> bool:
    text = entry_file.read_text(encoding="utf-8")
    out_spv = out_dir / (entry_file.stem + ".spv")
    cmd = [
        slangc, str(entry_file), "-o", str(out_spv), "-I", str(SHADERS_DIR),
        *entry_args(entry_file, text), "-profile", "sm_6_6", "-target", "spirv",
        "-O3", "-fvk-use-scalar-layout",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0


def run_shaders(apply: bool) -> None:
    slangc = find_slangc()
    entry_files = sorted(
        p for pattern in ("*.vert.slang", "*.frag.slang", "*.comp.slang")
        for p in SHADERS_DIR.rglob(pattern)
    )
    all_files = sorted(SHADERS_DIR.rglob("*.slang"))

    with tempfile.TemporaryDirectory() as tmp:
        out_dir = Path(tmp)

        print(f"Baseline compiling {len(entry_files)} entry shaders...")
        baseline_ok = {f for f in entry_files if compile_shader(slangc, f, out_dir)}
        broken = [f for f in entry_files if f not in baseline_ok]
        for f in broken:
            print(f"  SKIP (fails to compile before any changes): {f.relative_to(ROOT)}")

        removed_report: dict[Path, list[str]] = {}

        for shader_file in all_files:
            lines = shader_file.read_text(encoding="utf-8").splitlines(keepends=True)
            removable_idx = [i for i, l in enumerate(lines) if IMPORT_RE.match(l)]
            if not removable_idx:
                continue

            kept_removed = []
            for idx in removable_idx:
                original_line = lines[idx]
                lines[idx] = ""
                shader_file.write_text("".join(lines), encoding="utf-8")

                still_ok = all(compile_shader(slangc, f, out_dir) for f in baseline_ok)

                if still_ok:
                    kept_removed.append(original_line.strip())
                else:
                    lines[idx] = original_line
                    shader_file.write_text("".join(lines), encoding="utf-8")

            if kept_removed:
                removed_report[shader_file] = kept_removed

        if not apply:
            # Dry run: we mutated files transiently above to test compiles; make sure
            # everything is back exactly as it started by re-reading from git.
            subprocess.run(["git", "-C", str(ROOT), "checkout", "--", "shaders"], check=False)

        if not removed_report:
            print("No unused shader imports found.")
            return

        verb = "Removed" if apply else "Would remove"
        for f, lines_removed in removed_report.items():
            print(f"{verb} from {f.relative_to(ROOT)}:")
            for l in lines_removed:
                print(f"  - {l}")


def run_cpp(apply: bool) -> None:
    cl = find_cl()
    third_party = ROOT / "thirdParty"
    include_dirs = [
        SRC_DIR,
        third_party / "SDL3-3.4.10" / "include",
        third_party / "imgui",
        third_party / "imgui-filebrowser",
        third_party / "vkbootstrap",
        third_party / "glm",
        third_party / "magic_enum",
        third_party / "stb_image",
        third_party / "tinyexr",
        third_party / "vma",
        third_party,  # quill is included as quill/...
        third_party / "fastgltf" / "include",
    ]
    pch = SRC_DIR / "SwPch.h"
    if not pch.exists():
        candidates = list(SRC_DIR.rglob("SwPch.h"))
        if candidates:
            pch = candidates[0]

    defines = [
        "GLM_FORCE_DEPTH_ZERO_TO_ONE",
        'LOGS_PATH="."', 'ASSETS_PATH="."', 'LIGHTS_PATH="."',
        'SKYBOXES_PATH="."', 'DOCS_PATH="."', 'SHADERS_DIR="."',
    ]

    def compile_check(path: Path) -> bool:
        cmd = [
            cl, "/nologo", "/Zs", "/std:c++latest", "/EHsc", "/W0",
            f"/FI{pch}",
            *(f"/I{d}" for d in include_dirs),
            *(f"/D{d}" for d in defines),
            str(path),
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        return result.returncode == 0

    cpp_files = sorted(SRC_DIR.rglob("*.h")) + sorted(SRC_DIR.rglob("*.cpp"))
    removed_report: dict[Path, list[str]] = {}

    for src_file in cpp_files:
        if not compile_check(src_file):
            print(f"  SKIP (fails to compile standalone before any changes): {src_file.relative_to(ROOT)}")
            continue

        lines = src_file.read_text(encoding="utf-8").splitlines(keepends=True)
        removable_idx = [i for i, l in enumerate(lines) if INCLUDE_RE.match(l)]
        kept_removed = []

        for idx in removable_idx:
            original_line = lines[idx]
            lines[idx] = ""
            src_file.write_text("".join(lines), encoding="utf-8")

            if compile_check(src_file):
                kept_removed.append(original_line.strip())
            else:
                lines[idx] = original_line
                src_file.write_text("".join(lines), encoding="utf-8")

        if kept_removed:
            removed_report[src_file] = kept_removed
        if not apply and kept_removed:
            subprocess.run(["git", "-C", str(ROOT), "checkout", "--", str(src_file)], check=False)

    if not removed_report:
        print("No unused C++ includes found (within this best-effort check's limits).")
        return

    verb = "Removed" if apply else "Would remove"
    for f, lines_removed in removed_report.items():
        print(f"{verb} from {f.relative_to(ROOT)}:")
        for l in lines_removed:
            print(f"  - {l}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--shaders", action="store_true", help="scan shaders/ (reliable, uses real slangc)")
    parser.add_argument("--cpp", action="store_true", help="scan src/ (best-effort, uses cl.exe)")
    parser.add_argument("--apply", action="store_true", help="write removals; default is dry-run/report only")
    args = parser.parse_args()

    if not args.shaders and not args.cpp:
        parser.error("pass --shaders and/or --cpp")

    status = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True, text=True)
    if status.stdout.strip() and args.apply:
        sys.exit("Working tree has uncommitted changes. Commit or stash before running with --apply.")

    if args.shaders:
        run_shaders(args.apply)
    if args.cpp:
        run_cpp(args.apply)


if __name__ == "__main__":
    main()
