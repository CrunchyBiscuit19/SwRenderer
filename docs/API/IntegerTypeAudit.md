# Integer Type Audit — `std::size_t` / `std::uint64_t` candidates

Scan of `src/` for integer declarations where `std::size_t` or `std::uint64_t` (or
`vk::DeviceSize`, which is `std::uint64_t`) would be more correct than the current
`std::uint32_t` / `int`. Ordered by risk: the top group is an actual silent-truncation
bug waiting to happen, the lower groups are correctness-of-intent and future-proofing.

Container-`size()` casts to `std::uint32_t` that feed Vulkan `*Count` fields (e.g.
`SwDescriptor.cpp:56`, `SwCull.cpp:36`) are intentionally excluded: those Vulkan APIs
take `uint32_t`, so the narrowing is correct there.

---

## 1. Byte-offset accumulators declared `std::uint32_t` (highest risk)

These accumulate `vk::DeviceSize` (`uint64_t`) byte sizes into a `std::uint32_t`. The
`+=` silently truncates, and the value is then assigned back into a
`vk::BufferCopy::dstOffset` field that is itself `vk::DeviceSize`. Today the scene
vertex/index buffers are 256 MiB (`1 << 28`) so offsets happen to fit under 4 GiB, but
this is fragile against any growth of scene geometry.

| File:Line | Symbol | Current | Suggested |
| --- | --- | --- | --- |
| `src/Scene/SwScene.cpp:476` | `dstOffset` (vertex reload) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:477` | `maxPos` (vertex reload) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:498` | `dstOffset` (index reload) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:499` | `maxPos` (index reload) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:520` | `dstOffset` (material constants) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:540` | `dstOffset` (node transforms) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:560` | `dstOffset` (bounds) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Scene/SwScene.cpp:580` | `dstOffset` (instances) | `std::uint32_t` | `vk::DeviceSize` |
| `src/Data/SwAsset.cpp:525` | `dstOffset` (instances upload) | `std::uint32_t` | `vk::DeviceSize` |

Example (`src/Scene/SwScene.cpp:484-486`):

```cpp
meshVertexCopy.size = mesh.mNumVertices * sizeof(SwVertex); // size_t product -> vk::DeviceSize field
dstOffset += meshVertexCopy.size;                            // uint64 -> uint32 truncation
```

`meshVertexCopy.dstOffset` and `.size` are both `vk::DeviceSize`; only the loop
accumulator is narrow.

---

## 2. Buffer byte-size constants declared `std::uint32_t`

These are byte counts passed straight into `SwBufferFactory::createAllocatedBuffer(...,
std::uint64_t size, ...)`. The values fit today, but the type invites overflow the
moment anyone bumps an exponent (`1 << 31` becomes negative-ish / wraps, and any
`CONST * n` arithmetic overflows in 32-bit). Byte sizes should carry a 64-bit type.

| File:Line | Symbol | Current | Suggested |
| --- | --- | --- | --- |
| `src/Scene/SwScene.h:119` | `SCENE_INITIAL_VERTEX_BUFFER_SIZE` (`1 << 28`) | `std::uint32_t` | `std::uint64_t` / `vk::DeviceSize` |
| `src/Scene/SwScene.h:120` | `SCENE_INITIAL_INDEX_BUFFER_SIZE` (`1 << 28`) | `std::uint32_t` | `std::uint64_t` / `vk::DeviceSize` |
| `src/Data/SwBatch.h:44` | `RENDER_COMMANDS_INITIAL_BUFFER_SIZE` (`sizeof(...) * (1 << 10)`) | `std::uint32_t` | `std::size_t` / `std::uint64_t` |
| `src/Data/SwBatch.h:45` | `RENDER_ITEMS_INITIAL_BUFFER_SIZE` (`sizeof(...) * (1 << 13)`) | `std::uint32_t` | `std::size_t` / `std::uint64_t` |

Note on `SwBatch.h:44-45`: the initializer `sizeof(SwRenderCommand) * (1 << 10)` is a
`std::size_t` expression that is then narrowed into a `std::uint32_t` constant, so the
declared type is already fighting its own initializer.

The other `SCENE_INITIAL_NUM_*` constants in `SwScene.h:121-126` are element *counts*,
not byte sizes, and are legitimately `std::uint32_t`. Where they appear as
`COUNT * sizeof(T)` (e.g. `SwScene.cpp:97,114`) the product already promotes to
`std::size_t`, so no change is needed there — only the standalone byte-size constants
above.

---

## 3. `size() * sizeof(...)` byte size cast down to `std::uint32_t`

| File:Line | Expression | Current | Suggested |
| --- | --- | --- | --- |
| `src/System/SwIBL.cpp:227` | `skyboxVertexSize = static_cast<std::uint32_t>(mSkyboxVertices.size() * sizeof(float))` | `std::uint32_t` | `std::uint64_t` / `vk::DeviceSize` |

The source expression is a `std::size_t` byte count; the explicit narrowing cast is only
safe because the skybox is tiny. Since this value is used as a buffer size it should stay
64-bit and drop the cast.

---

## 4. Already correct — kept as positive references

No change needed; listed so the audit is complete and these are not flagged later.

- `src/Resource/SwShader.cpp:17` — `const size_t fileSize = file.tellg();` correctly uses
  `size_t` for a file byte length.
- `src/Resource/SwSampler.h:48-62` — `std::hash` specialization uses `std::size_t` for
  the seed and `hashCombine` value, which is the right width for a hash.
- `src/Resource/SwBuffer.h` — the whole buffer API (`mSize`, `resize`, `copyFrom`,
  `ensureCapacity`, `DeferredBuffer::mFrameQueued`) already uses `std::uint64_t`.
- `src/Renderer/SwStagingRing.h` — offsets, capacity, head/tail, and frame counters are
  all `std::uint64_t`. This is the model the `dstOffset` accumulators in section 1 should
  follow.

---

### Summary

The one substantive issue is **section 1**: byte-offset accumulators typed
`std::uint32_t` while everything around them (copy sizes, `vk::BufferCopy` fields, the
buffers themselves) is 64-bit. Sections 2–3 are lower-severity type-of-intent fixes that
harden against future size increases.
