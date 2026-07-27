# 12 - Fixed-function per-draw cost vs gl4es

Same-machine comparison against gl4es 1.1.7, NVIDIA GTX 1660 SUPER, both
libraries on the same GLES backend, viewport 1x1 so fragment work is out of
the measurement, best of 3 (harness: `tests/bench_cmp_gl4es.c`).

SFPEW column re-measured after the `perf` merge (below); the gl4es column is
from the first run of the same harness on the same machine - gl4es is unchanged
code, so only the ratios were recomputed.

| phase | SFPEW before | SFPEW now | gl4es | ratio now |
|---|---|---|---|---|
| tinybatch (1 quad per Begin/End) | 3.51 us | 0.89 us/batch | 0.35 us | 2.6x slower |
| dlist replay | 8.7 | 7.3 ns/vert | 9.1 | 1.25x FASTER |
| clientarrays | 34.2 | 7.2 ns/vert | 6.0 | 1.2x slower |
| drawelements | 14.0 | 8.5 ns/idx | 3.9 | 2.2x slower |
| texswitch | 4.04 | 3.43 us/draw | 2.28 | 1.5x slower |
| progtoggle | 4.36 | 3.70 us/draw | 5.18 | 1.4x FASTER |
| immediate | 85.9 | 52.5 ns/vert | 117.3 | 2.2x FASTER |
| matrixops | 96.4 | 101.9 ns/group | 182.8 | 1.8x FASTER |
| getter | 10.6 | 11.8 ns/call | 89.3 | 7.6x FASTER |

The merge closed **clientarrays** (5.7x slower -> 1.2x, essentially parity) and
halved the **drawelements** gap; deferring the draw-state restore then took
**tinybatch** from 8.7x to 2.6x. Six of nine phases beat gl4es and the worst
remaining gap is 2.6x, down from 10x when this file was started.

What is left is small and structural. The wrapper cannot reach gl4es on
tinybatch because gl4es owns all GL state while this wrapper has to coexist
with the host's own GL/GLES calls, so a fixed-function draw still has to leave
the app's bindings as it found them - now once per run of draws rather than once
per draw, but never zero.

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
6. **Deferred state restore flushed from `sfpewEnsureBackend`.** That
   *placement* is ruled out, not the idea. It measured as no gain (tinybatch
   3.51 -> 3.34 us, noise) for a structural reason: the fixed-function entry
   points call `sfpewEnsureBackend` too, so `glVertex3f`/`glEnd` flushed the
   deferred state between draws and it was re-bound exactly as before. The
   same is true of `flushPendingImmediateDraws()` (109 call sites, the
   codebase's existing "something else is happening" barrier) - `glBegin`
   calls it, so hooking there self-defeats identically. See "The prize" below
   for what the idea is actually worth with a correct flush placement.

7. **The ring's rotating offset.** Pinning the immediate-mode upload offset to
   zero (an unsafe measurement, reverted) made tinybatch 4x WORSE, 3.9 ->
   16.8 us: overwriting a region the GPU is still reading stalls. The
   rotating offset is what prevents that, so the per-draw `glBindVertexBuffer`
   it forces is buying something real and is not the cost to remove.

## Where the time actually goes

Re-profiled after the merge (`perf record --call-graph dwarf`, tinybatch only),
by shared object:

| | share of tinybatch |
|---|---|
| `libnvidia-eglcore` | 56.7% |
| kernel | 15.1% |
| `libSimpleFPEWrapper` | 13.6% |
| `libc` (mostly the ring `memmove`) | 5.8% |
| `ld-linux` (TLS descriptor resolution) | 4.8% |

~72% is driver-side command submission. The wrapper's own 13.6% is fragmented
with no hotspot above 2.1% (`send_vertex_attributes` 2.1%, `advance()` 1.7%,
`drawImmediateVertices` 1.6%, `program_hash` 1.6%), so there is nothing left to
win by micro-optimizing wrapper code. The lever is the *number* of driver calls
per draw, which is what the deferred restore above attacks.

One dead end found here and reverted: the 4.8% in `ld-linux` is real
(`_dl_tlsdesc_dynamic_xsave` 2.65% + `__tls_get_addr` 0.94%), caused by a small
draw touching ~6 separate `thread_local` blocks under the default
general-dynamic TLS model. Marking the hot ones
`__attribute__((tls_model("initial-exec")))` does remove it, but glibc must
then fit the module's *entire* TLS block in the static-TLS surplus, and this
library's is 9152 bytes (6144 of it one 256-entry display-list cache) against a
~1664-byte default surplus - so `smoke_dlopen` fails outright with "cannot
allocate memory in static TLS block", and dlopen is how the library is normally
loaded. Not worth shrinking three caches to chase 3.6% while tinybatch is
still 8.7x off.

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

## The prize: what deferred restore is actually worth

Measured directly by disabling the work rather than by guessing. Two unsafe
build hacks (both reverted; they existed only to bound the win):

| tinybatch, NVIDIA, viewport 1x1 | us/batch |
|---|---|
| as shipped | 3.29 - 3.46 |
| guard destructor made a no-op (restores skipped) | 2.55 - 2.72 |
| ...and the setup re-binds skipped when already ours | 1.01 - 1.11 |

So deferred restore is worth **~3.3x on tinybatch** (3.4 -> 1.03 us), which
would move it from 9.7x slower than gl4es to ~2.9x. Roughly 7 driver calls per
batch disappear at ~340ns each on this driver, which is consistent with the
56.7% of the profile that sits in `libnvidia-eglcore`.

Both halves are needed and they are the same mechanism: skipping the restore is
only 22% on its own, because the next draw re-binds anyway. Keeping the
fixed-function program/VAO/buffer bound *across* consecutive draws is what
removes both.

What makes it safe is the flush placement, and the observer set is smaller than
it first looks: `glGetIntegerv(GL_CURRENT_PROGRAM)` and
`GL_ARRAY_BUFFER_BINDING` are already answered from the logical shadows, which
track the app's view and which the FPE draw path deliberately never touches -
those getters stay correct for free. What genuinely observes is the
VBO/VAO/shader API surface (draws through the passthrough path,
`glVertexAttribPointer`, `glBufferData`/`SubData`/`Map`, the three bind
entries, backend-passthrough queries of `GL_VERTEX_ARRAY_BINDING` /
`GL_ELEMENT_ARRAY_BUFFER_BINDING`) plus frame/context boundaries, plus
deletion of a name held in the pending save (which must invalidate, not
flush - restoring a deleted name resurrects it).

### LANDED

tinybatch **3.04 -> 0.89 us/batch**, slightly better than the 1.03 us bound
predicted above, taking it from 8.7x slower than gl4es to 2.6x. No other phase
moved. Two halves, both needed:

1. `fpe_backend_draw_state_guard_t` no longer restores in its destructor. The
   save moves to `glstate_t::deferred_draw` and is replayed by
   `sfpewEntryBarrier()`, which every exported entry point outside the
   immediate-mode vertex family calls first. The default is inverted to safe:
   a barrier that is not needed costs the restore it would have paid anyway,
   so only glBegin/glEnd and the glVertex/glColor/glNormal/glTexCoord/... calls
   keep the bare `flushPendingImmediateDraws()`.
2. `glstate_t::immediate_live_program` lets `drawImmediateVertices` skip its
   `glUseProgram` + `glBindVertexArray` + `glBindBuffer` when the previous
   immediate draw already left that trio bound. Skipping the restore alone was
   only 22%; the re-binds are the other half.

The flag is deliberately narrow, because the two failure modes are not
symmetric: failing to SET it just costs speed, while failing to CLEAR it skips
a bind that was needed. So the clear lives inside
`sfpewBackendBindVertexArray()` (which every FPE path and the app's own
`glBindVertexArray` already route through) plus
`sfpewInvalidateImmediateDrawState()` at the handful of places that bind a
program without touching a VAO - `SET_PREV_PROGRAM`, pixelops, the shader
object helpers, `glUseProgram`, `commit_fpe_state_on_draw`. Three raw
`glBindVertexArray(fpe_user_vao)` calls were converted to the helper for the
same reason.

**The bug this shook out**, and the one worth remembering: the logical program
shadow self-heals every 256 queries by asking the backend for
`GL_CURRENT_PROGRAM`. With the restore deferred, that query sees the WRAPPER's
program and poisons the shadow with an internal id - after which every
fixed-function draw took the user-program path and passed `GL_QUADS` straight
to GLES, so `clientarrays` and `gatherarrays` dropped every draw and raised
`GL_INVALID_ENUM`. The same applies to the array-buffer and VAO shadows. A
reconcile against the backend is only meaningful while the backend actually
holds the app's state, so all three now answer from `deferred_draw` while it is
held. Any future shadow that heals off a backend query needs the same
treatment.

Policing: `smoke_vbo_surface` (phase C2), `smoke_render`, `smoke_mixed_pipeline`
and `bench.fpe` all fail if the barrier is disabled - verified by temporarily
stubbing it out. `bench_fpe.c` now reports GL errors per phase instead of one
"something failed" at the end, which is what localized the shadow bug.

### The prerequisite this needed: intercepting the buffer surface

Attempted and reverted, for a reason no flush placement can fix. An observer
only gets a flush if the wrapper *sees* the call, and several entry points that
read or write `GL_ARRAY_BUFFER` / the bound VAO are neither wrapped nor handed
out by `eglGetProcAddress`, so they fall through to the backend's own function
pointer and the wrapper never observes them at all:

`glBufferData`, `glBufferSubData`, `glVertexAttribPointer`,
`glVertexAttribIPointer`, `glEnableVertexAttribArray`,
`glDisableVertexAttribArray`, `glMapBufferRange`, `glUnmapBuffer`,
`glGetBufferParameteriv`.

Wrapped in d125f79. Before that it was harmless, because the guard restored
before every entry point returned, so between wrapper calls the app's bindings
were always correct. Under deferred restore it would have been silent corruption - a fixed-function draw leaves the wrapper's
ring buffer and VAO bound, then the app's own `glBufferData(GL_ARRAY_BUFFER, …)`
writes into the wrapper's ring, and its `glVertexAttribPointer` configures the
wrapper's VAO instead of its own. That is exactly the LWJGL/VBO frontend
014436b added support for, so it is not a hypothetical.

Note this is already a latent inconsistency independent of deferred restore:
the wrapper shadows the `GL_ARRAY_BUFFER` binding (`glBindBuffer` is wrapped)
but cannot see writes through that binding.

Also worth noting independently of the perf work: the ARB spellings were routed
to the backend by `GETPROC_BACKEND_ALIAS` even for entry points the wrapper does
implement, and LWJGL2 asks for those by preference. `GETPROC_WRAPPER_ALIAS` now
distinguishes the two cases.

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
