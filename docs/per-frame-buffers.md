# Per-Frame Buffers: When To Multi-Buffer a GPU Resource

Rules of thumb for deciding whether a GPU buffer or image needs one instance per
frame-in-flight. Getting this wrong produces intermittent, frame-rate-dependent flicker
that hides at low FPS and reappears when the frame rate rises.

## The invariant

A GPU resource that the CPU writes each frame, or that one frame's GPU work overwrites while
an earlier frame's GPU work may still be reading it, must have a separate instance per
frame-in-flight. Otherwise the two overlapping frames race on the same memory.

## Why this renderer overlaps frames

`NUM_FRAME_OVERLAP` is 2 (`SwSwapchain.h`). `getCurrentFrame()` returns
`mFrames[mFrameNumber % 2]`, and `SwScene::draw()` waits only on the current slot's render
fence before recording. That fence was last signaled two frames ago, so frame N waits on
frame N-2, never on frame N-1. Frame N-1's GPU work can therefore still be in flight while
frame N is being recorded and submitted.

The render fence bounds the GPU to at most two frames behind the CPU. It does not prevent
frame N and frame N-1 from overlapping. Nothing in a frame's command buffer synchronizes
against the previous frame's command buffer. Barriers resolve hazards inside one frame only.
Raising the frame rate widens the overlap window, which is why these races surface as a
symptom of a performance improvement rather than a code change to the racy resource itself.

## Rules of thumb

Ask these in order. A yes to either of the first two means the resource needs one instance
per frame-in-flight.

1. **Does the CPU write it every frame (or on an event that can fire during steady-state
   rendering)?** Host-written per-frame data races the GPU read from the previous frame.
   Example: the camera buffer, the render-command and render-item source buffers, the scene
   lights buffer.

2. **Does the GPU overwrite it early in the frame and consume it later in the same frame,
   every frame?** The next frame's early write can land on the GPU before the current
   frame's late read completes. Example: the per-batch early and late render-command buffers
   and their count buffers, which `SwCull` zeros and refills every frame and `SwGeometry`
   reads through `drawIndexedIndirectCount`.

3. **Is it only ever produced and consumed within a single frame's command buffer, with no
   cross-frame reuse of its contents?** Then a single instance is fine. Render-graph barriers
   cover it. A transient scratch buffer that is fully written before it is read inside the
   same frame does not need duplication, as long as the next frame fully rewrites it before
   reading rather than reading stale contents.

4. **Is it immutable after upload, or rebuilt only at resize / load time behind a
   `waitIdle`?** Then a single instance is fine. Vertex, index, material-constant, node, and
   bounds buffers are uploaded once and only read on the GPU, so they are single-buffered.
   Anything rebuilt inside a `mDevice.waitIdle()` block (resize) is safe because no frames
   are in flight at that point.

## The decision in one question

Can frame N's GPU reads of this resource still be running when frame N+1 writes it? If yes,
duplicate per frame. The fence only guarantees frame N is done before frame N+2, so two
instances indexed by frame parity are sufficient at `NUM_FRAME_OVERLAP` of 2.

## Worked inventory

| Resource | Written by | Read by | Per-frame? | Why |
| --- | --- | --- | --- | --- |
| Camera buffer | CPU each frame | Cull, geometry | Yes, one per `SwFrame` | Host write vs previous-frame GPU read |
| `mInitialRcsBuffer`, `mRisBuffer` | CPU on load, cull each frame | Cull | Yes, array of 2 | Host write plus per-frame cull scratch |
| Scene lights buffer | CPU on light edit | Lighting, geometry | Yes, array of 2 | Host write vs previous-frame GPU read |
| Early / late RCS buffers and counts | Cull each frame | Geometry indirect draw | Yes | Next frame's reset zeros them mid-draw of the current frame |
| Vertex, index, material, node, bounds buffers | CPU once at load | GPU read only | No | Immutable after upload |
| Depth pyramid, visibility RIS ping-pong | GPU, cross-frame by design | GPU | Special | Intentional previous-frame data flow, not stomp avoidance |

The last row is a distinct pattern. A ping-pong (read previous, write current) and the
single reprojected depth pyramid deliberately carry data across the frame boundary. They are
not duplicated to avoid a race, they are structured so the previous frame's result is the
input. Treat those cases on their own terms rather than by these rules.

## The implementation pattern

Two idioms exist in the codebase, both correct.

- **One instance inside each `SwFrame`.** The frame owns its command buffer, fences,
  semaphores, and camera buffer. Natural when the resource is conceptually part of the frame.

- **`std::array<T, 2>` indexed by frame parity.** Allocate two, and route every access through
  an accessor that returns element `SwRenderer::sRendererContext.mSwapchain->getFrameNumber() % 2`.
  Because the frame number increments once per loop iteration, every access inside one frame
  (both the `perFrameUpdate` system refresh and the command buffer recorded in `draw`)
  resolves to the same instance, and consecutive frames resolve to different instances. This
  is how `mInitialRcsBuffer`, `mRisBuffer`, and the scene lights buffer are duplicated. Keep
  the accessor out of line so callers do not need to know it is duplicated, which leaves every
  existing call site unchanged.

For variable-size per-frame uploads, prefer `SwStagingRing` over N fixed copies. It tags each
sub-allocation with the frame it was recorded in and reclaims it after `NUM_FRAME_OVERLAP`
frames retire, which generalizes per-frame duplication to a single growable arena.

## Cost note

Duplicating a resource multiplies its allocation by `NUM_FRAME_OVERLAP`. Only duplicate what
the rules above flag. Do not duplicate immutable or single-frame-transient resources, and do
not reach for duplication when the correct answer is intentional cross-frame data flow.
