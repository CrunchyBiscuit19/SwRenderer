# Buffer-Size Constant Consistency Flags

> **Status: resolved.** All count-form buffer constants were converted to explicit
> byte-size constants named `*_BUFFER_SIZE` (with `sizeof` folded into the definition and
> dropped at the call site). The two constants that are genuinely counts —
> `SCENE_INITIAL_NUM_MATERIALS` (also a descriptor-array count) and
> `SCENE_INITIAL_NUM_RENDER_ITEMS` (sizes both a `std::uint32_t` and a `SwRenderItem`
> buffer) — were retained, and derived byte-size constants
> (`SCENE_INITIAL_MATERIAL_CONSTANTS_BUFFER_SIZE`, `SCENE_INITIAL_RENDER_ITEMS_BUFFER_SIZE`,
> `SCENE_INITIAL_RIS_INDICES_BUFFER_SIZE`) were added alongside them. Only
> `INITIAL_ACTIVE_LIGHTS_BUFFER_SIZE` (FLAG 4) is left as-is pending an intent decision on
> its element size. The original analysis is kept below for reference.

Every `constexpr` / `static constexpr` that seeds an initial GPU buffer allocation, split
by what its value actually *means*. There are two competing conventions in the codebase:

- **Byte-size form** — the constant already holds a byte count and is passed straight into
  `createAllocatedBuffer(...)` / `createStagingBuffer(...)` / `initialize(...)`.
- **Count form** — the constant holds an element count and the byte size is computed at the
  call site as `CONST * sizeof(T)`.

The mix is not itself a bug, but the same underlying quantity is expressed both ways, and
the naming does not reliably signal which form a given constant uses. Flags below.

---

## Inventory

### Byte-size form (value is bytes, passed raw)

| Constant | Location | Value | Passed at |
| --- | --- | --- | --- |
| `SCENE_INITIAL_VERTEX_BUFFER_SIZE` | `SwScene.h:121` | `1ull << 28` | `SwScene.cpp:54` |
| `SCENE_INITIAL_INDEX_BUFFER_SIZE` | `SwScene.h:122` | `1ull << 28` | `SwScene.cpp:58` |
| `RENDER_COMMANDS_INITIAL_BUFFER_SIZE` | `SwBatch.h:44` | `sizeof(SwRenderCommand) * (1 << 10)` | `SwBatch.cpp:18,26,38` |
| `RENDER_ITEMS_INITIAL_BUFFER_SIZE` | `SwBatch.h:45` | `sizeof(SwRenderItem) * (1 << 13)` | `SwBatch.cpp:48` |
| `DATA_BUFFER_SIZE` | `SwSwapchain.h:27` | `sizeof(Data)` | `SwSwapchain.cpp:16` |
| `INITIAL_CAPACITY` | `SwStagingRing.h:31` | `1 << 4` | `SwRenderer.cpp:197` |
| `INITIAL_ACTIVE_LIGHTS_BUFFER_SIZE` | `SwLighting.h:42` | `1 << 10` | `SwLighting.cpp:37` |

### Count form (value is element count, `* sizeof(T)` at call site)

| Constant | Location | Value | Multiplied at |
| --- | --- | --- | --- |
| `SCENE_INITIAL_NUM_MATERIALS` | `SwScene.h:123` | `1 << 8` | `SwScene.cpp:65` |
| `SCENE_INITIAL_NUM_NODES` | `SwScene.h:124` | `1 << 12` | `SwScene.cpp:73` |
| `SCENE_INITIAL_NUM_INSTANCES` | `SwScene.h:125` | `1 << 8` | `SwScene.cpp:81` |
| `SCENE_INITIAL_NUM_BOUNDS` | `SwScene.h:126` | `1 << 12` | `SwScene.cpp:89` |
| `SCENE_INITIAL_NUM_RENDER_ITEMS` | `SwScene.h:127` | `1 << 18` | `SwScene.cpp:97,114` |
| `SCENE_INITIAL_NUM_LIGHTS` | `SwScene.h:128` | `1 << 6` | `SwScene.cpp:105` |
| `SHADOW_INITIAL_RENDER_COMMANDS` | `SwLighting.h:30` | `1 << 10` | `SwLighting.cpp:106` |
| `NUM_ASSET_MATERIALS` | `SwAsset.h:14` | `1 << 6` | `SwAsset.cpp:106` |
| `NUM_ASSET_BOUNDS` | `SwAsset.h:16` | `1 << 10` | `SwAsset.cpp:112` |
| `NUM_ASSET_NODES` | `SwAsset.h:15` | `1 << 10` | `SwAsset.cpp:118` |
| `NUM_ASSET_INSTANCES` | `SwAsset.h:13` | `1 << 6` | `SwAsset.cpp:124` |

---

## Flags

### FLAG 1 — Same concept, opposite conventions: "render commands"

`RENDER_COMMANDS_INITIAL_BUFFER_SIZE` (`SwBatch.h:44`) is a **byte size** that folds both
the element size and a `1 << 10` count into the constant, whereas
`SHADOW_INITIAL_RENDER_COMMANDS` (`SwLighting.h:30`) is a **count** of `1 << 10` render
commands multiplied by `sizeof(SwRenderCommand)` at the call site. Two constants describe
the same thing — an initial render-command buffer of 1024 commands — in two different
forms with two different naming schemes. A reader cannot tell they are equivalent.

### FLAG 2 — Same concept, opposite conventions: "render items"

`RENDER_ITEMS_INITIAL_BUFFER_SIZE` (`SwBatch.h:45`, byte size, count folded in) versus
`SCENE_INITIAL_NUM_RENDER_ITEMS` (`SwScene.h:127`, count multiplied at call site). Again
the same "render items" quantity, expressed with opposite conventions in sibling systems.

### FLAG 3 — Mixed conventions inside one class (`SwScene`)

`SwScene.h:121-128` declares one contiguous block where the first two entries
(`..._VERTEX_BUFFER_SIZE`, `..._INDEX_BUFFER_SIZE`) are byte sizes and the remaining six
(`SCENE_INITIAL_NUM_*`) are counts. The `_BUFFER_SIZE` vs `_NUM_` suffix is the only cue,
and the vertex/index pair is the exception to the count convention used everywhere else in
the same class.

### FLAG 4 — Misleading name, ambiguous unit: `INITIAL_ACTIVE_LIGHTS_BUFFER_SIZE`

`SwLighting.h:42` names this `_BUFFER_SIZE` (byte convention) but its value `1 << 10`
carries no `sizeof`, so it reads like a count. It is passed raw at `SwLighting.cpp:37`, so
the `VisibleLightsBuffer` is allocated as literally **1024 bytes**. This is inconsistent on
two axes at once:

- Against `SHADOW_INITIAL_RENDER_COMMANDS` in the *same file*, which is also `1 << 10` but
  is treated as a count (`* sizeof`).
- Against its own name, which promises a byte size but whose value looks like an entry count.

The buffer is `resizable = true`, so this will not overflow — it grows on demand — but the
1024-byte initial size is almost certainly not the intended "1024 lights." Owner should
confirm whether this wants `INITIAL_ACTIVE_LIGHTS * sizeof(entry)` (count form) or an
honest byte literal.

---

## Suggested direction (not applied)

Pick one convention per allocation site and make the name match the unit:

- Count constants: name `NUM_*` or `*_COUNT`, keep the `* sizeof(T)` at the call site.
- Byte constants: name `*_BUFFER_SIZE` / `*_BYTES`, and where a count is meaningful write
  it as `NUM * sizeof(T)` in the initializer (as `RENDER_COMMANDS_INITIAL_BUFFER_SIZE`
  already does) rather than a bare power of two.

The most actionable item is **FLAG 4**, which is a likely latent under-allocation rather
than only a naming issue.
