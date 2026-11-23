# Browser structural-support scratch reuse — November 23, 2026

## Problem

Each browser collapse support pass allocated a new visited-cell bitmap and a
new breadth-first queue, and each isolated island allocated a fresh DFS stack.
Repeated buildings collapsing can therefore create avoidable garbage
collection work and frame-time spikes.

## Change

- Cache the visited Uint8Array across calls. Zero it for the next collapse,
  and resize only when switching between 120-column and 360-column worlds.
- Reuse the BFS queue and component DFS stack; clear their lengths on each
  traversal. Keep the live component voxel arrays independent, since they
  become actual physics chunks.
- Preserve the original material/support tests, voxel coordinates, collision
  rules, level content, and progression. The C core, DS, 3DS and Dreamcast
  code is unchanged.
- The optimization does not add new physics scans or extra graphics buffers.

## Verification

The test/browser_support_cache_test.js production-code regression covers two
successive detachments, repeated empty passes, stable scratch identities,
a 60-voxel-wide component in the 360-column finale, and a return to 120 columns.
It also syntax-checks the complete bundled browser script.
The test is part of test/run_tests.sh.

This trades short-lived allocations for a persistent scratch bitmap of about
7 KiB on normal maps or 21 KiB on a 360-column map, plus two reused JS arrays.
It is not a measured FPS gain. Hardware and browser worst-case frame-time
profiling are still outstanding.
