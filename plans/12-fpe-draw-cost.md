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
   3.51 us. On this driver those queries are client-side. The shadow itself
   now lives on `glstate_t` (`backend_vao_binding`, healed every 256 draws)
   after the `perf` merge; the `glBindVertexArray` wrapper this branch added
   is what keeps it exact when the app binds its own VAO.
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

7. **The ring's rotating offset.** Pinning the immediate-mode upload offset to
   zero (an unsafe measurement, reverted) made tinybatch 4x WORSE, 3.9 ->
   16.8 us: overwriting a region the GPU is still reading stalls. The
   rotating offset is what prevents that, so the per-draw `glBindVertexBuffer`
   it forces is buying something real and is not the cost to remove.

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

The seven ruled-out items above narrow this to one lever. gl4es feeds
vertices as client-side arrays: the driver copies from client memory when the
draw is issued, and on this driver that is cheaper for a small draw than
binding a freshly written region of a persistently mapped ring. Client arrays
are legal in GLES only with the default VAO, while this path deliberately
binds `fpe_user_vao` so it never disturbs the app's attribute state - and the
guard saves the VAO *binding*, not the per-attribute state inside VAO 0.
Moving to client arrays therefore means saving and restoring per-attribute
state on VAO 0, which is more state per draw, not less, unless it is combined
with the deferred restore above. That combination is the only remaining route
to parity on these phases, and it needs both risky pieces at once.

Candidate directions:

- **Deferred restore.** Keep the fixed-function program/VAO/buffers bound
  across consecutive fixed-function draws and flush back to the app's state
  only at entry points that could observe program, VAO or buffer bindings.
  This removes both the restores and the re-binds. The risk is completeness:
  the set of observing entry points has to be exhaustive, and missing one
  is a hard-to-trace state corruption rather than a visible failure.
- **Display-list compilation.** DONE, but via the `perf` branch's design, not
  this branch's. Both were written independently: 40d23a7 compiled the block
  at `glEnd` (suppressing per-call recording), while `perf`'s 1665878 records
  normally and compiles whole Begin/End runs at `glEndList`. The latter won on
  the merge because compiling later keeps the recorded commands available, so a
  run can fall back to them and can inherit call-time current attributes -
  which 40d23a7 could not, and which `smoke_list_current_attr` pins down (a
  colorless list must draw in the color current at `glCallList` time). It also
  merges adjacent runs into one draw. dlist 87.5 -> 8.7 ns/vert, overtaking
  gl4es (9.1). Covered by `smoke_list_immediate` + `smoke_list_current_attr`.

## Merged with the `perf` branch's sprint

The same problem was worked twice in parallel: this branch measured against
gl4es and attacked the per-draw path, while the `perf` branch profiled and
found a different, larger root cause - `eglGetCurrentContext` costs ~425ns per
call on glvnd desktops (getpid fork check + dispatch mutex), and the wrapper
paid it on every textual `g_glstate` use, ~83% of all cycles. Its fix was one
strict context resolve per exported entry with a thread-local snapshot
downstream (`docs/context-model.md`), plus ring-buffer client uploads, a cached
`advance()` copy plan, `glEndList` run compilation, and shader source-hash
memoization.

Those 8 commits are merged in. Where the two branches had built the same
mechanism, the resolution was:

- **Context tracking.** Kept both, composed: `perf`'s snapshot architecture,
  but the strict resolve calls this branch's `sfpewCurrentContext()`, which
  returns a plain TLS read when the app routes `eglMakeCurrent` through the
  wrapper (de48412 added that interception, which `perf` assumed impossible)
  and falls back to polling libEGL otherwise. So the resolve is cheaper than
  either branch had it alone, and `SFPEW_RELAXED_CONTEXT=1` still skips it.
- **VAO shadow.** `perf`'s (`glstate_t::backend_vao_binding`, healed in the
  draw guard) over this branch's separate `logical_vertex_array_state_t`, but
  this branch's `glBindVertexArray`/`glDeleteVertexArrays` wrappers were kept
  and repointed at it - without them the shadow only converges on the
  every-256-draws heal, so an app binding its own VAO gets that many draws
  restoring the wrong one.
- **Display-list compilation.** `perf`'s `glEndList` compiler; see above.
- **Vertex packing.** `perf` rewrote `advance()` into a cached span-copy plan
  but built the plan from vertex/normal/color/texcoord only, silently
  reintroducing the 8d19e85 bug (slots 5 and 6 sized but never packed shifts
  every later attribute). The fog-coord and secondary-color spans are added
  back in `rebuild_packed_layout()`, matching `compile_vertexattrib()`'s
  declaration order.

ctest 670/670 on the merged tree. Measured side by side against the `perf`
build on this machine, the merge is at parity or better on every phase
(tinybatch 2.15 vs 3.31 us, drawelements 51.6 vs 56.2 ns/idx, dlist 72.3 vs
73.8 ns/vert) - those absolute numbers are a different GPU routing than the
table above, so compare them only to each other.

## Caveats on the numbers

gl4es drives its own GLX to an X root window while this wrapper uses an EGL
pbuffer, so the driver submission paths are not identical; gl4es also
reports GL 2.1 on a GLES 2.0 backend and skips the ES3 work (VAOs, vertex
attribute formats) this path does. The 1x1 viewport removes fragment cost
but not these differences, so treat the absolute ratios as approximate. The
"gap is per-draw fixed cost" conclusion comes from the profile and from the
phase split above, and does not depend on them.
