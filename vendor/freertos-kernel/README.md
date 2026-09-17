# Vendored FreeRTOS-Kernel

Source: https://github.com/FreeRTOS/FreeRTOS-Kernel
Commit: `8be86d4a24fd4091f8f4192018423ab590f408db` (2026-08-26)
License: MIT — see [LICENSE.md](LICENSE.md), unmodified from upstream.

## Why files are copied in, not a git submodule

Upstream's `portable/` tree includes a RISC-V port
(`portable/GCC/RISC-V/chip_specific_extensions/...`) with a path long enough
to fail a plain `git clone`/submodule checkout on Windows without
`core.longpaths` enabled — confirmed while setting this up. Rather than
depend on every contributor's Windows git config, or fight submodule sparse-
checkout (which is local machine state, not something `.gitmodules` can pin
for everyone), this vendors only the specific files this project actually
needs.

## What's included

- Core kernel: `tasks.c`, `queue.c`, `list.c`, `timers.c`, `croutine.c`,
  `event_groups.c`, `stream_buffer.c`, `include/*.h`
- Port: `portable/GCC/ARM_CM7/r0p1/` — the Cortex-M7 GCC port. Per
  FreeRTOS's own docs, `r0p1` is correct for all Cortex-M7 silicon
  revisions (it contains an errata workaround that's harmless on later
  revisions), not just r0p1 parts specifically.
- Heap: `portable/MemMang/heap_4.c` — coalescing allocator with free-block
  merging, the standard general-purpose choice.

## Upgrading

Re-copy the same file set from a fresh upstream checkout (with
`core.longpaths` enabled locally, or sparse-checked-out to avoid the
RISC-V path issue), update the commit hash above, and diff before
committing — this is a manual, deliberate step, not an automated pull.
