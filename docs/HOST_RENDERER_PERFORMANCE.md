# Host Renderer Performance Pass

This pass targets the Windows OpenGL simulator. It removes repeated driver and
allocation work from the draw path while preserving the GameCube data model.
The comparison baseline is libPorpoise commit `b5632a8` (`Add GX renderer
performance benchmark`).

## Reference audit

The implementation was informed by the following pinned revisions. The source
was used as design evidence; no third-party source was copied into libPorpoise.

| Project | Relevant design | Local decision |
| --- | --- | --- |
| [Dolphin `f1b4345d`](https://github.com/dolphin-emu/dolphin/tree/f1b4345d0854f1c4e5e8eb683237a771c6de5456) | `OGLStreamBuffer.cpp` uses capability-selected mapped stream buffers, ring allocation, and fences. Shader managers track dirty constants, and the texture cache avoids redundant decoding with content hashes. | Add a fenced persistent vertex ring with a mutable fallback, exact uniform-value dirty tracking, and unchanged-texture validation. |
| [Gecko `39e82205`](https://github.com/ioncodes/gecko/tree/39e82205a0da154f23fd36b95e64a8029d468618) | `flipper/gx/bp.rs` has targeted dirty slots and texture hashes. `vertex.rs` decodes into renderer scratch and uses cached specialized parsers from `flipper/gx/jit.rs`. | Narrow uniform invalidation to state actually consumed by GLSL and avoid unchanged texture work. Direct-to-final-buffer decode and parser specialization remain future work. |
| [Lazuli `4f8110e7`](https://github.com/vxpm/lazuli/tree/4f8110e7e04ac38b0cf3c3241617afffdb4b2ff4) | `stream.rs` keeps binary command traffic explicitly big-endian. Its renderer selects mappable primary buffers or staging, and `vtxjit` caches parsers by vertex configuration. | Keep endian conversion at command/texture boundaries and capability-gate the mapped upload path. Vertex JIT remains future work. |
| [PureiKyubu `554e9e74`](https://github.com/emu-russia/pureikyubu/tree/554e9e74816c49b126e43023309268ac3cb64abb) | `gfx.h` explicitly lacks texture caching, its modern VBO path is incomplete, and the optional draw path still performs per-draw `glBufferData`. It does precompute vertex sizes and separates command processing onto a thread. | Treat this as a negative control for the upload pattern, not as the renderer design to follow. |
| [Aurora `6c4c27f`](https://github.com/encounter/aurora/tree/6c4c27f9e8e40f584d27726655d80ec85a5a7d2c) | `command_processor.cpp` deduplicates passive BP writes but exempts command register `0x52`, caches array uploads, and merges draws. `dl.cpp` batches display-list draws; `gfx/common.cpp` rotates mapped staging buffers; `shader_info.cpp` packs shader-specific uniforms. | Preserve `0x52` command semantics while deduplicating passive state, and use rotating mapped storage. Draw merging and shader-specific uniform packing remain future work. |

## Implemented changes

### Persistent vertex stream

The host renderer now uses three persistently mapped coherent pages of 65,535
`RenderVertex` elements when `ARB_buffer_storage` and the required entry points
are available. Page transitions fence the page just submitted and wait before a
page is reused. If storage or mapping fails, the immutable buffer is replaced
with a normal mutable OpenGL 3.3 buffer before falling back to `glBufferData`.

Triangle lists larger than one page are split only at triangle boundaries.
Other oversized host-only inputs use a separate mutable overflow VAO/VBO so
their topology is not silently changed or discarded.

### Uniform invalidation and value cache

`GlobalState` now exposes a uniform-state revision which advances only for BP,
XF, and texture-descriptor state that feeds the GLSL payload. The renderer skips
payload construction when this revision and the drawable dimensions are
unchanged. When it does rebuild, an exact per-field cache issues only the
necessary `glUniform*` calls. Uniform locations remain cached per linked program
and can be explicitly invalidated after a same-name program relink.

First writes of raw zero values are tracked separately from the value itself.
Repeated BP `0x52` writes remain command-like: a regression test proves that the
same command can update the semantic reference width from 800 to 640 after the
copy-source state changes.

### Allocation-free texture validation

`TextureContentSnapshot::Matches` no longer constructs and copies a temporary
full-size vector. Canonical GameCube bytes use `memcmp`; SDK-facing native
`u16` data is compared against the canonical snapshot with an unaligned-safe,
word-wise byte swap on little-endian hosts. Texture and TLUT capture and matching
share the same encoding rules.

### Release delivery and performance gates

The Windows and Linux standalone CI scripts now configure Meson with
`--buildtype=release`. Opt-in benchmarks cover:

- automatic persistent streaming;
- forced legacy uploads;
- forced persistent-map failure and mutable fallback;
- post-timing framebuffer readback of the measured draw path;
- four-page ring wrap/reuse with framebuffer marker readback; and
- canonical and native-`u16` texture matching with allocation counting.

## Endian contract

GX FIFO words and canonical texture/TLUT snapshots remain big-endian.
Conversion occurs at explicit input boundaries. `RenderVertex` and
`ShaderUniformValues` are already decoded host-native data, so they are copied
to OpenGL without byte swapping.

Cross-encoding tests run on little-endian Windows and prove that canonical
big-endian bytes and native numeric `u16` values compare identically. This pass
does not claim that the host renderer ran on GameCube hardware or on a
big-endian host.

## Measurements

Measurements were taken on 2026-08-01 on Windows 10.0.19045, an AMD Ryzen 7
3700X, and an NVIDIA GeForce RTX 3070 with Windows driver `32.0.15.6094`.
Both baseline and current binaries used GCC 16.1.0 and Meson `release` builds.
Each renderer figure is the median of nine 2,000-draw trials with 192 vertices
per draw, with baseline, persistent, and forced-legacy runs interleaved in each
trial.

| Renderer path | Median microseconds/draw | Timed uniform calls | Timed `glBufferData` calls |
| --- | ---: | ---: | ---: |
| Baseline commit `b5632a8` | 14.051 | 70,000 | 2,000 |
| Current, forced legacy upload | 13.589 | 0 | 2,000 |
| Current, persistent upload | 10.848 | 0 | 0 |

The complete current path is 22.8% lower latency than the exact pre-change
commit in this workload. Comparing the two current modes isolates persistent
streaming at 20.2% lower latency. The forced-map-failure mode also completed all
2,000 mutable uploads, proving that optional buffer-storage failure remains
functional.

The 4 MiB texture snapshot benchmark, also over nine trials of 64 comparisons,
measured medians of 165.819 microseconds for canonical bytes and 460.011
microseconds for native `u16` data. Both paths performed zero allocations inside
`Matches`.

The standalone approximately-117-FPS result produced during investigation is
intentionally excluded: its current binary had frame pacing removed while its
baseline retained pacing, so it was not a controlled comparison.

## Reproduction

Configure and run the opt-in release benchmarks:

```sh
meson setup .build/perf-release --buildtype=release \
  -Dbuild_target=win64 -Dtests=disabled -Dbenchmarks=enabled
meson test -C .build/perf-release --benchmark --print-errorlogs --verbose
```

The automatic, legacy, map-failure, and texture cases are registered as four
separate Meson benchmarks. Automatic and map-failure modes report a Meson skip
when the OpenGL context lacks the complete buffer-storage/sync capability set;
legacy mode still exercises the mutable fallback. For a historical comparison,
build commit `b5632a8` with the same compiler and options and run:

```sh
.build/perf-release/benchmarks/gx-renderer-benchmark 2000 192
```

The verification gate for this pass was:

- 48 of 48 release host tests passing, including CodeWarrior source compatibility,
  Wii compatibility, and GX/endian tests;
- 4 of 4 release benchmark modes passing;
- a fresh release standalone host configure and build succeeding; and
- both standalone CI scripts passing `bash -n` with the bundled MSYS2 Bash.

No Pikmin build or game-runtime result is included.

## Next performance work

The next highest-value experiments are direct vertex decode into final upload
scratch, a smaller host vertex representation, cached specialized vertex
parsers, compatible adjacent-draw merging, and shader-specific packed uniform
buffers. Texture dirty-page tracking could also avoid an O(n) content comparison
when the emulated memory system can prove that a source range was not written.
