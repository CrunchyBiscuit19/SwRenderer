#!/usr/bin/env python3
# Cross checks the data structures shared between the Slang shaders and the C++ headers.
# The two sides do not need to declare the same set of structs. A struct is treated as shared
# when both sides declare it under the identical qualified name (for example SwMaterial::Constant
# or the top-level SwVertex), and every shared struct must list its fields with the same names in
# the same order, each using an equivalent type (for example uint in Slang against std::uint32_t
# in C++).

import os
import re
import sys

SHADER_ROOT = "shaders"
HEADER_ROOTS = ["src"]

# Leading declaration keywords that carry no type information and are dropped before parsing.
QUALIFIERS = {
    "public", "private", "protected", "static", "const", "constexpr", "mutable", "inline",
    "nointerpolation", "in", "out", "inout", "uniform", "volatile", "groupshared", "centroid",
    "linear", "sample", "row_major", "column_major", "globallycoherent",
}

# Primitive scalar and vector types reduced to a common canonical tag per language.
SLANG_CANON = {
    "float": "F32", "double": "F64", "half": "F16",
    "int": "I32", "uint": "U32", "int64_t": "I64", "uint64_t": "U64",
    "bool": "BOOL",
    "float2": "VEC2", "float3": "VEC3", "float4": "VEC4",
    "int2": "IVEC2", "int3": "IVEC3", "int4": "IVEC4",
    "uint2": "UVEC2", "uint3": "UVEC3", "uint4": "UVEC4",
    "float2x2": "MAT2", "float3x3": "MAT3", "float4x4": "MAT4",
}
CPP_CANON = {
    "float": "F32", "double": "F64",
    "int": "I32", "std::int32_t": "I32", "int32_t": "I32",
    "unsigned": "U32", "unsigned int": "U32", "std::uint32_t": "U32", "uint32_t": "U32",
    "std::int64_t": "I64", "int64_t": "I64", "std::uint64_t": "U64", "uint64_t": "U64",
    "bool": "BOOL",
    "glm::vec2": "VEC2", "glm::vec3": "VEC3", "glm::vec4": "VEC4",
    "glm::ivec2": "IVEC2", "glm::ivec3": "IVEC3", "glm::ivec4": "IVEC4",
    "glm::uvec2": "UVEC2", "glm::uvec3": "UVEC3", "glm::uvec4": "UVEC4",
    "glm::mat2": "MAT2", "glm::mat3": "MAT3", "glm::mat4": "MAT4",
}
# A C++ device address stands in for any buffer pointer declared on the Slang side.
CPP_POINTER_TYPES = {"vk::DeviceAddress", "vk::DeviceSize"}

OPENER = re.compile(r"\b(namespace|struct|class|enum)\b(?:\s+(?:class|struct))?\s+(~?\w+)[^{;()]*\{")


def clean(text):
    # Strips comments, literals and preprocessor lines so brace counting stays balanced.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    text = re.sub(r'"(\\.|[^"\\])*"', '""', text)
    text = re.sub(r"'(\\.|[^'\\])*'", "''", text)
    text = re.sub(r"(?m)^\s*#.*$", "", text)
    return text


def strip_nested_braces(inner):
    # Collapses every nested brace group so only the direct members of a scope remain.
    while True:
        reduced = re.sub(r"\{[^{}]*\}", " ", inner)
        if reduced == inner:
            return inner
        inner = reduced


def scan_scopes(text):
    # Walks matched braces and yields (kind, qualified_name_parts, inner_text) for every
    # struct, class and enum, including nested ones.
    openers = {}
    for m in OPENER.finditer(text):
        openers[m.end() - 1] = (m.group(1), m.group(2))

    stack = []
    scopes = []
    for m in re.finditer(r"[{}]", text):
        pos = m.start()
        if m.group() == "{":
            if pos in openers:
                kind, name = openers[pos]
                qual = [f["name"] for f in stack if f["kind"] in ("namespace", "struct", "class")]
                stack.append({"kind": kind, "name": name, "open": pos, "qual": qual})
            else:
                stack.append({"kind": "block", "open": pos})
        else:
            if not stack:
                continue
            top = stack.pop()
            if top["kind"] in ("struct", "class", "enum"):
                inner = text[top["open"] + 1:pos]
                scopes.append((top["kind"], top["qual"] + [top["name"]], inner))
    return scopes


def parse_fields(inner):
    inner = strip_nested_braces(inner)
    inner = re.sub(r"\b(public|private|protected)\s*:", " ", inner)  # Drops C++ access-specifier labels.
    fields = []
    for stmt in inner.split(";"):
        s = stmt.strip()
        if not s or "(" in s or ")" in s:
            continue
        s = s.split("=")[0].strip()
        s = re.sub(r"\s:\s.*$", "", s).strip()  # Drops a Slang semantic binding such as : SV_Position.
        s = re.sub(r"^\s*\[[^\]]*\]\s*", "", s)  # Drops a leading attribute block.
        toks = s.split()
        while toks and toks[0] in QUALIFIERS:
            toks.pop(0)
        if len(toks) < 2:
            continue
        name = toks[-1]
        typ = " ".join(toks[:-1])
        if name.startswith("*"):
            typ += " *"
            name = name.lstrip("*")
        name = re.sub(r"\[[^\]]*\]$", "", name)
        if not re.match(r"m[A-Za-z_]\w*$", name):
            continue
        fields.append((typ.strip(), name))
    return fields


def parse_enumerators(inner):
    inner = strip_nested_braces(inner)
    names = []
    for part in inner.split(","):
        p = part.strip().split("=")[0].strip()
        m = re.match(r"(\w+)", p)
        if m:
            names.append(m.group(1))
    return names


class Item:
    def __init__(self, kind, qual, inner, file):
        self.kind = kind
        self.qual = qual
        self.file = file
        self.name = "::".join(qual)
        if kind == "enum":
            self.fields = []
            self.enumerators = parse_enumerators(inner)
        else:
            self.fields = parse_fields(inner)
            self.enumerators = []


def collect(roots, exts):
    items = []
    for root in roots:
        for dirpath, _, files in os.walk(root):
            for f in files:
                if not f.endswith(exts):
                    continue
                path = os.path.join(dirpath, f)
                with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                    text = clean(fh.read())
                rel = os.path.relpath(path).replace("\\", "/")
                for kind, qual, inner in scan_scopes(text):
                    items.append(Item(kind, qual, inner, rel))
    return items


def canonical_type(raw, is_slang, enum_shorts):
    # Reduces a field type to (category, tag) so the two languages can be compared.
    raw = " ".join(t for t in raw.split() if t not in ("const", "volatile"))
    if "*" in raw:
        return ("PTR", None)
    if not is_slang and raw in CPP_POINTER_TYPES:
        return ("PTR", None)
    table = SLANG_CANON if is_slang else CPP_CANON
    if raw in table:
        return ("PRIM", table[raw])
    short = raw.split("::")[-1]
    if raw in enum_shorts or short in enum_shorts:
        return ("PRIM", "U32")  # Enums lower to a 32 bit unsigned integer on both sides.
    return ("STRUCT", raw)  # Struct types share the identical name across both languages.


def types_equivalent(a, b):
    if a[0] == "PTR" or b[0] == "PTR":
        return a[0] == b[0]
    return a == b


def pair(slang_items, cpp_items):
    # Pairs Slang and C++ items that declare the identical qualified name and kind.
    cpp_by_key = {}
    for i, c in enumerate(cpp_items):
        cpp_by_key.setdefault((c.kind, tuple(c.qual)), []).append(i)
    pairs = []
    used = set()
    ambiguous = []
    unmatched_slang = []
    for s in slang_items:
        candidates = [i for i in cpp_by_key.get((s.kind, tuple(s.qual)), []) if i not in used]
        if not candidates:
            unmatched_slang.append(s)
            continue
        if len(candidates) > 1:
            ambiguous.append((s, [cpp_items[i].name for i in candidates]))
            continue
        idx = candidates[0]
        used.add(idx)
        pairs.append((s, cpp_items[idx]))
    unmatched_cpp = [c for i, c in enumerate(cpp_items) if i not in used]
    return pairs, ambiguous, unmatched_slang, unmatched_cpp


def compare_pair(s, c, enum_shorts):
    problems = []
    if s.kind == "enum":
        if s.enumerators != c.enumerators:
            problems.append("enumerators differ: slang %s vs cpp %s" % (s.enumerators, c.enumerators))
        return problems
    n = max(len(s.fields), len(c.fields))
    for i in range(n):
        sf = s.fields[i] if i < len(s.fields) else None
        cf = c.fields[i] if i < len(c.fields) else None
        if sf is None:
            problems.append("field %d: missing in slang, cpp has %s %s" % (i, cf[0], cf[1]))
            continue
        if cf is None:
            problems.append("field %d: missing in cpp, slang has %s %s" % (i, sf[0], sf[1]))
            continue
        if sf[1] != cf[1]:
            problems.append("field %d: name mismatch, slang '%s' vs cpp '%s'" % (i, sf[1], cf[1]))
            continue
        st = canonical_type(sf[0], True, enum_shorts)
        ct = canonical_type(cf[0], False, enum_shorts)
        if not types_equivalent(st, ct):
            problems.append("field '%s': type mismatch, slang '%s' vs cpp '%s'" % (sf[1], sf[0], cf[0]))
    return problems


def main():
    verbose = "-v" in sys.argv or "--verbose" in sys.argv
    slang_items = collect([SHADER_ROOT], (".slang",))
    cpp_items = collect(HEADER_ROOTS, (".h", ".hpp"))

    enum_shorts = set()
    for it in slang_items + cpp_items:
        if it.kind == "enum":
            enum_shorts.add(it.name)
            enum_shorts.add(it.qual[-1])

    pairs, ambiguous, unmatched_slang, unmatched_cpp = pair(slang_items, cpp_items)

    failures = 0
    print("Checked %d Slang and %d C++ structs/enums, matched %d pairs.\n"
          % (len(slang_items), len(cpp_items), len(pairs)))

    for s, c in sorted(pairs, key=lambda p: p[0].name):
        problems = compare_pair(s, c, enum_shorts)
        if problems:
            failures += 1
            print("MISMATCH  %s  (%s)" % (s.name, s.file))
            print("      vs  %s  (%s)" % (c.name, c.file))
            for p in problems:
                print("          - " + p)
            print()
        elif verbose:
            print("ok        %s  <->  %s" % (s.name, c.name))

    if ambiguous:
        print("\nAmbiguous pairings (add an explicit rule or rename):")
        for s, cands in ambiguous:
            print("  %s (%s) could be %s" % (s.name, s.file, ", ".join(cands)))

    if verbose:
        if unmatched_slang:
            print("\nSlang-only structs (no C++ counterpart, informational):")
            for s in sorted(unmatched_slang, key=lambda i: i.name):
                print("  %s (%s)" % (s.name, s.file))
        if unmatched_cpp:
            print("\nC++-only structs (no Slang counterpart, informational):")
            for c in sorted(unmatched_cpp, key=lambda i: i.name):
                print("  %s (%s)" % (c.name, c.file))

    print("\n%d mismatched pair(s)." % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
