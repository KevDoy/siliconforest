## Goal
Working audio: headphone jack first, then built-in speakers under a hard safety cap, then microphones.

## Decision
**Native IOKit drivers on PureDarwin's `IOAudioFamily`; do not port the Linux drivers.**
- Linux's drivers sit on ASoC, regmap and dmaengine, none of which exist in our compatibility layer, and the result would still need a shim into `IOAudioFamily`.
- PureDarwin already has Apple's audio family, an example driver (`RavynHDAudio`, 1,148 lines), `coreaudiod`, and CoreAudio/AudioToolbox frameworks.
- Use OpenBSD's Apple audio drivers (ISC; `aplmca`, `apldma`, `aplaudio`, `tascodec`) as the structural model **after confirming they exist in a checkout** (the research cites them from memory), with Linux and m1n1 as register references. Anything derived from GPL files stays GPL-2.0.

## Hardware on the J293 (from the Linux device tree)
- **Speakers:** four TI TAS5770L amplifiers on two I2C buses, shared shutdown GPIO 181 and interrupt GPIO 182, current/voltage sense returned in TDM slots.
- **Headphone jack:** Cirrus CS42L83 on i2c2 at 0x48 (reset GPIO, interrupt GPIO 183); also the headset microphone.
- **Transport:** Apple MCA I2S block with NCO clocks and the ADMAC DMA controller behind `dart-sio`.
- **Built-in microphones:** through the AOP coprocessor (RTKit), not the MCA.
- No firmware or calibration files are needed on T8103.

## Safety (read this before touching speakers)
The amplifiers can damage the speakers under software control. Linux enables speakers only for listed models, caps amplifier gain, and holds volume at the minimum unless a userspace process is actively modelling speaker temperature from the sense channels (`macaudio.c`). Our rule:
1. Headphones first. GPIO 181 stays low so the amplifiers remain in shutdown.
2. Speakers are a later work item and must ship a hard kernel cap (fixed low amplifier gain, digital volume clamp, no user override) and verified sense capture **before** any sound.
3. Damage paths to design against: wrong TDM slot or format producing full-scale noise, DC output, a DMA underrun looping a buffer.

## Work items
| ID | Deliverable | Acceptance on the M1 | Size |
|---|---|---|---|
| I1 | Confirm ADT node and property names for codecs, MCA, ADMAC on this machine (kexts match Apple's tree, not Linux's) | Table of nodes committed | 1 day |
| I2 | Codec presence over I2C (needs SF-A A6, A7) | CS42L83 device ID and each amplifier's revision read back | small |
| I3 | Transport engine: NCO, MCA, ADMAC ring in DART-mapped memory, as an `IOAudioEngine` | `pcmplay` runs with a monotonic sample clock and no DART faults, codec muted | ~1,500 lines |
| I4 | CS42L83 playback, jack detection, volume | 1 kHz tone in headphones at 44.1 and 48 kHz; plug/unplug logged | ~800 lines |
| I5 | `coreaudiod` on the M1: default device, volume persistence | A tone plays through the CoreAudio API | small |
| I6 | Speakers behind the hard cap, sense capture verified | Quiet tone per speaker; sense slots carry plausible data | ~700 lines + safety review |
| I7 | Capture: headset microphone, then built-in microphones via AOP (needs capture support in `coreaudiod`, which has none) | Record and play back a clip | largest unknown |

## Dependencies
SF-A (A2 power domains, A5 DART instance for `dart-sio`, A6 GPIO, A7 I2C; A3 RTKit for I7).

## Interfaces this ticket owns
Audio kexts under `integrations/puredarwin/m1-audio/`; `coreaudiod` capture path.

## Risks
`IOAudioFamily` has never run in our arm64 kernel collection. Clocking and TDM details are easy to get wrong (see Linux `macaudio.c` format constants).

## Research
`docs/roadmap/research/sound-bluetooth.md`, part A.
