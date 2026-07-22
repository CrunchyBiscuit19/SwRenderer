# Segmented Compaction for the Shared Post-Cull RC Buffer

This note explains the segmented compaction approach for the single shared post-cull render command (RC)
buffer, and why it is preferred over dense global compaction. It is a design reference for the upcoming
`SwCull` / `SwBatch` step and is not yet implemented.

## What compaction does and why

After the cull work pass, every render command in `mInitialRcsBuffer` has had its `mRiCount` (which
doubles as the indirect draw's `instanceCount`) set to the number of visible instances for that draw. A
fully-culled RC ends up with `mRiCount == 0`.

You could issue one big `drawIndexedIndirectCount` over all RCs and let the 0-instance ones no-op, but the
command processor still fetches and steps over every one of those structs. Compaction removes the dead RCs
so the draw only walks live ones. The count buffer tells `drawIndexedIndirectCount` how many live structs
there actually are.

So compaction reads the initial RC list and writes a second list containing only survivors, plus a count.

## The layout problem

With one shared buffer, the question is where in the output each batch's survivors land. Take three batches
in the shared initial buffer:

```
initial buffer (12 slots):
 idx:   0   1   2   3 | 4   5   6 | 7   8   9  10  11
 batch: [----B0-----] | [--B1--]  | [------B2------]
 base:  rcsOffset=0     rcsOffset=4  rcsOffset=7
```

Say the cull pass leaves these survivors: B0: idx 0,2 (2 live). B1: idx 5 (1 live). B2: idx 7,9,10 (3 live).

### Dense compaction

Dense compaction packs everything to the front of the output:

```
 idx:   0    1    2    3    4    5
 rc:   rc0  rc2  rc5  rc7  rc9  rc10
 batch:[-B0-]    [B1] [----B2----]
```

B0 starts at 0, B1 at 2, B2 at 3. Those start offsets (2, 3) depend on how many earlier batches survived,
which is only known after the GPU runs. The draw call needs that offset as a CPU parameter, and you do not
have it without reading GPU memory back. That is the wall, because `vkCmdDrawIndexedIndirectCount`'s buffer
offset is a CPU-supplied parameter, not something it can read from GPU memory.

### Segmented compaction

Segmented compaction packs each batch densely within its own original slot, keeping the slot's base at the
batch's stable `rcsOffset`:

```
 idx:   0    1    2    3  | 4    5    6  | 7    8    9   10   11
 rc:   rc0  rc2   .    .  | rc5  .    .  | rc7  rc9  rc10  .    .
       [-live-][-stale-]  |[live][stale]|[----live----][-stale-]
 count[0]=2                 count[1]=1     count[2]=3
```

B0's survivors go to `[0,1]` (base 0), B1's to `[4]` (base 4), B2's to `[7,8,9]` (base 7). The tails
(`buffer[2,3]`, `buffer[5,6]`, `buffer[10,11]`) hold leftover garbage, but nothing ever reads them because
each draw only reads `count_B` structs starting at the base.

The whole point is that each batch's base offset is identical to its initial-buffer offset, which is
CPU-known and stable every frame. Only the three `count` values are produced by the GPU.

## How the draw consumes it

Each batch draws from its fixed base, with its dynamic count:

```
// B0:  drawIndexedIndirectCount(post, 0*stride, count, 0*4, maxDraws=4, stride)  -> count[0]=2
// B1:  drawIndexedIndirectCount(post, 4*stride, count, 1*4, maxDraws=3, stride)  -> count[1]=1
// B2:  drawIndexedIndirectCount(post, 7*stride, count, 2*4, maxDraws=5, stride)  -> count[2]=3
```

`maxDraws` is the batch's `rcsSize` (the upper bound, since you cannot survive more RCs than you started
with), and `count` clamps it to the actual survivor number at draw time. Buffer offset, count-slot offset,
and maxDraws are all CPU-known and constant; only the count value changes per frame.

## How the compaction shader writes segmented

This falls out almost for free because the compaction already dispatches once per batch. You pass the base
and count slot as push constants:

```
// push constants per batch: preBase = rcsOffset, postBase = rcsOffset, limit = rcsSize, count slot address
uint i = threadId;                 // 0 .. rcsSize-1
if (i >= limit) return;
RenderCommand rc = initial[preBase + i];
if (rc.mRiCount == 0) return;      // culled, skip
uint local;
InterlockedAdd(*countSlot, 1, local);
post[postBase + local] = rc;       // dense within this batch's slot
```

Because `preBase` / `postBase` come in per dispatch, the shader never has to figure out which batch a thread
belongs to, since the dispatch is the batch. This is why the single-global-dispatch variant is harder: there,
one dispatch spans all batches, so each thread would need a lookup from its global RC index back to its
segment base and count slot. With per-batch dispatch that lookup is just a push constant.

## What new bookkeeping this adds

Only two small things versus the current setup.

1. A per-batch count slot. `mEarlyRcsCount` / `mLateRcsCount` need `numBatches` uints (one per
   batch) instead of the old per-batch single-count buffers. Reset them to 0 before the work pass.
2. A batch ordinal. Each `SwBatch` needs its index into that count array. Assign a running counter during the
   flatten loop in `regenerateRcsAndRis` (0, 1, 2, ... in the same order you emit batches) and store it next
   to the offsets.

## Why the memory cost is nil

Segmented keeps the post-cull buffer as large as the full initial buffer (holes and all) rather than
shrinking to the survivor count. But the early / late buffers already call `ensureCapacity(rcsBytes)`, sized
to all RCs, so reserving each batch's full slot costs nothing beyond what is already allocated. You trade a
bit of unused buffer space for draw offsets you can compute on the CPU, which is exactly the constraint
`drawIndexedIndirectCount` imposes.

## Summary

The whole scheme reduces to two pieces of data per batch.

1. An offset into the buffer. This is just the batch's `rcsOffset`, its index into the `mRcs` vector,
   turned into a byte offset with `rcsOffset * sizeof(SwRenderCommand)`. Because the post-cull buffer reuses
   the initial layout, the same offset addresses the batch in both the initial and the compacted buffer, and
   it is stable across frames.
2. A separate count buffer with one slot per batch, indexed by the batch's ordinal. The compaction shader
   does an atomic add into that slot as it writes survivors, and the draw reads the slot back as its count.

Nothing else needs to pass between the CPU and the GPU. The offset is derived on the CPU from the vector
layout, and the count is the only value the GPU produces per batch.
