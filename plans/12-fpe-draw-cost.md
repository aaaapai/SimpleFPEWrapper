# 12 - Fixed-function per-draw cost vs gl4es

Same-machine comparison against gl4es 1.1.7, NVIDIA GTX 1660 SUPER, both
libraries on the same GLES backend, viewport 1x1 so fragment work is out of
the measurement, best of 3 (harness: `tests/bench_cmp_gl4es.c`).

| phase | SFPEW | gl4es | ratio |
|---|---|---|---|
| tinybatch (1 quad per Begin/End) | 3.51 us/batch | 0.35 us/batch | 10.0x slower |
| dlist replay | 8.7 ns/vert | 9.1 ns/vert | 1.05x FASTER (was 9.6x slower) |
| clientarrays | 34.2 ns/vert | 6.0 ns/vert | 5.7x slower |
| drawelements | 14.0 ns/idx | 3.9 ns/idx | 3.6x slower |
| texswitch | 4.04 us/draw | 2.28 us/draw | 1.8x slower |
| progtoggle | 4.36 us/draw | 5.18 us/draw | 1.2x FASTER |
| immediate | 85.9 ns/vert | 117.3 ns/vert | 1.4x FASTER |
| matrixops | 96.4 ns/group | 182.8 ns/group | 1.9x FASTER |
| getter | 10.6 ns/call | 89.3 ns/call | 8.4x FASTER |

The split is clean: pure CPU state work is ahead, and every phase that is
behind shares the one draw-submission path. So the gap is a fixed per-draw
cost, not per-vertex throughput.

## Ruled out by measurement

Do not re-try these; each was implemented or checked and did not move the
number.

1. **The guard's synchronous state queries.** Shadowing the VAO binding and
   dropping the element-buffer query (ed4a0b9) moved tinybatch 3.58 ->
   3.51 us. On this driver those queries are client-side.
2. **Buffer orphaning per draw.** The `glBufferData` in `uploadImmediate`
   is only the fallback; `GL_EXT_buffer_storage` is present here, so the
   persistent-mapped ring is active and the upload is a `memcpy`.
3. **Fence stalls in the ring.** Fences are taken only when an upload
   crosses a quarter of the ring; a 144-byte tiny batch almost never does.
4. **Uniform re-submission.** All 19 `glUniform*` calls in `send_uniforms`
   are already gated on a change flag.

5. **Vertex attribute setup per draw.** Already cached: `send_vertex_attributes`
   returns early on `!va.dirty`, and each attribute's format/binding is
   compared against `fpe_vertex_attributes` before being re-sent.
6. **Deferred state restore, flushed from `sfpewEnsureBackend`.** Implemented
   and reverted: no measurable gain (tinybatch 3.51 -> 3.34 us, within
   noise). The reason is structural - the fixed-function entry points call
   `sfpewEnsureBackend` too, so `glVertex3f`/`glEnd` flush the deferred state
   between draws and it is restored and re-bound exactly as before. A working
   version has to place the flush only on the non-fixed-function entry points
   (passthrough, getters, user-program draws), which is the exhaustive
   enumeration that makes this option risky: a missed observer is silent
   state corruption, not a test failure.

## Where the time actually goes

`perf` with call graphs puts 54% of tinybatch inside `glEnd` ->
`drawImmediateVertices`, of which the largest single leaf is an NVIDIA
driver function (~16%) reached from the draw, plus ~8% in `__ioctl` and
~12% in the kernel. That is driver-side command submission, not wrapper
CPU time - consistent with the wrapper issuing more GL calls per draw than
gl4es does, rather than doing more work per call.

## The architectural difference

gl4es is a complete libGL and owns all GL state, so its internal draws
neither save nor restore anything, and it feeds vertices as client-side
arrays (legal in GLES with the default VAO) with no upload and no VAO
switch. This wrapper must coexist with an app that also issues its own
GLES/GL3 calls, so each fixed-function draw currently binds its own VAO and
buffer, points attributes at them, switches program, and then puts the
app's program, VAO and array buffer back - a fixed set of driver calls per
draw that gl4es simply does not make.

Two candidate directions, both real changes rather than tuning:

- **Deferred restore.** Keep the fixed-function program/VAO/buffers bound
  across consecutive fixed-function draws and flush back to the app's state
  only at entry points that could observe program, VAO or buffer bindings.
  This removes both the restores and the re-binds. The risk is completeness:
  the set of observing entry points has to be exhaustive, and missing one
  is a hard-to-trace state corruption rather than a visible failure.
- **Display-list compilation.** DONE (40d23a7). `glBegin` suppresses
  per-call recording and `glEnd` emits one command holding the accumulated
  vertex block, so replay is one upload and one draw instead of a command per
  vertex attribute call. dlist 87.5 -> 8.7 ns/vert, which overtakes gl4es
  (9.1). Covered by `smoke_list_immediate`.

## Caveats on the numbers

gl4es drives its own GLX to an X root window while this wrapper uses an EGL
pbuffer, so the driver submission paths are not identical; gl4es also
reports GL 2.1 on a GLES 2.0 backend and skips the ES3 work (VAOs, vertex
attribute formats) this path does. The 1x1 viewport removes fragment cost
but not these differences, so treat the absolute ratios as approximate. The
"gap is per-draw fixed cost" conclusion comes from the profile and from the
phase split above, and does not depend on them.
