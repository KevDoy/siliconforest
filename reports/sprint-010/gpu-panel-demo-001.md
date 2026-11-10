# GPU demo on the built-in panel, with timings (Darwin vs Linux on the same M1)

Boot `pd01` (10 November 2026): `AGX_TEMPLATE=.../m1-mesa-asahi/supervisor/darwin-agx-panel-demo.in
tools/agx_boot.sh pd01`, Wi-Fi + GPU collection. `agx-panel-demo` renders three scenes at
2560x1600 on the GPU (GLES 3, off-screen target), reads the frame back and copies it to the panel
through the existing framebuffer mapping. 21 runs of 30 frames, all completed, exit 0, 17.8 s.
Nobody was watching the panel; what is proven is that the frames were rendered, read back and
written to the panel mapping, not what the eye saw.

Median time per frame (ms). Darwin runs on one E core under the hypervisor; Linux uses all cores.

| Scene | GPU render only | + readback | + present to panel (best path) |
|---|---|---|---|
| Plasma shader, Darwin | 1.9 | 10.3 | 26.8 (37 fps) |
| Plasma shader, Linux | 0.9 | 2.1 | 4.2 (to heap memory, no panel) |
| 10,000 blended quads, Darwin | 3.1 | 14.7 | 31.2 (32 fps) |
| 10,000 blended quads, Linux | 2.1 | 4.9 | 9.8 |
| Lit mesh with depth, Darwin | 1.8 | 10.1 | 26.7 (37 fps) |
| Lit mesh with depth, Linux | 0.9 | 2.3 | 3.8 |

Reading: the GPU itself draws a full 2560x1600 frame in 2-3 ms under Darwin. The cost is getting
16 MB to the screen through the CPU: about 8 ms to read back and 16-24 ms to write the panel
mapping on one E core. The old CPU compositor needed 290 ms per full frame, the damage-tracked
one 37 ms. Faster presentation needs the display controller path (scanout of GPU buffers,
ticket SF-H), more CPU cores (SF-C), or both.

Correctness: the two readback paths agree with each other in every scene. Of the six
full-resolution frame hashes, five equal Linux's; one differs: the mesh scene on the 10-bit
target (Darwin `b27160c9...`, Linux `fd04cc04...`, stable over three Linux runs). Cause unknown;
not yet known how many pixels differ or whether Darwin's value is stable. Open defect.
