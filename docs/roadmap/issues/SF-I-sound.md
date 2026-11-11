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

## Status (November 11)
Headphone-jack playback runs on the M1 from a native kext, `PDAppleSound` (`integrations/puredarwin/m1-sound/`, GPL-2.0), without `IOAudioFamily`: boots `sd01` to `sd04` (8 CPUs, DEVELOPMENT kernel). Nobody has listened yet, and the jack was empty in every boot (Linux reports the same).
- **Done on hardware:** I1 (node table below), I2 for the CS42L83 only (device ID `0x42a83`, revision `0xb0`), the transport of I3 as a polled DMA ring (not an `IOAudioEngine`), the playback path of I4 at 44.1 and 48 kHz with jack state polled every 2 s and volume.
- **Unattended evidence:** the codec's PLL locks to the bit clock, its input sample-rate converter reports lock and its serial port reports no framing error while data flows; the DMA controller's 24 MHz timestamps give 48000.02 Hz and 44100.02 Hz; no DART fault, no I2C error.
- **Measured:** the codec's input converter locks 1.04 s (48 kHz) to 1.16 s (44.1 kHz) after a start; the driver plays silence until then (`kern.pd_sound.preroll_ms`). Whether the codec would pass sound before the lock is not known.
- **Jack (boot `sd05`):** the codec's interrupt (GPIO pin 183, AIC 190) drives plug, unplug, headset type and button detection, ported from Omacom's `cs42l42.c` including the CS42L83 Apple-remote receiver. Proven without a plug: the interrupt line fires (`delivered (1 interrupts, 1 services, 0 found by looking at the pin)`) and one type detection runs end to end on the empty jack. Real plug, type and button events need a person: `guest/darwin-sound-listen-jack.in`.
- **Speakers, kernel side (boots `sd06`, `sd07`):** the four TAS5770L amplifiers (revision `0x50`, die 25-31 C), the 8-slot TDM stream from cluster 1 to the ports of clusters 0 and 1, and the current/voltage sense capture on cluster 2 run from `pd_tas2770` and `pd_speakers` (ported from Omacom `tas2770.c` and the J293 policy of `macaudio.c`). Safety as Linux has it, enforced in the kernel: gain fixed at `0x0f` (18.5 dBV), every volume held at -100 dB unless the safety daemon holds `/dev/pdspeaker-safety` and pings every 250 ms of playback; on top of Linux, a fault bit, a register that reads back wrong, an I2C error, a DMA ring error or dry ring, or a DART fault puts the amplifiers into hardware shutdown, and after three such stops the speakers stay off. Proven: `pdspk-test` passes 13 lock checks (request above the lock refused, lease expiry after 400 ms without a ping, `ETIMEDOUT` once, close locks), registers read back `0xc8`/`0x0f` while playing, 48000.015 Hz, sense stream delivered (noise only at -100 dB: peaks of 30-60 of 32768), forced shutdown path three times then disabled. **Nothing audible has been played and none will be without the daemon**; the sense channels have not been seen carrying a signal.
- **Safety daemon (boot `sd08`):** `m1-sound/safetyd/` is Asahi's speakersafetyd 2.0.1 (MIT, Rust) with its ALSA layer replaced by `/dev/pdspeaker-safety`; model and main loop are upstream's (host tests: the gain trajectory is bit-identical to upstream's `types.rs`, 16 failure paths end the process). On the M1 it starts, takes the lock as on Linux (`speaker volumes unlocked`, volumes to 0 dB on a cold start) and waits for sense data; killing it locks at once. It has never run while the speakers played: that is the attended step, `guest/darwin-sound-listen-speakers.in`.
- **System audio (boots `sd20`, `sd21`):** the tree's `coreaudiod` with a `/dev` backend (`m1-sound/sysaudio/`): AudioQueue clients, two clients mixed at 44.1 and 48 kHz, volume and mute through CoreAudio mapped to the codec, `pdvolume`; no IOAudioFamily.
- **Headset microphone (boot `sd10`):** `/dev/pdmic` captures the codec's converter through the jack port's receiver (cluster 0, second receiver) while the headphone stream runs. With the jack empty: 140 periods in 3 s, both channels the same, offset about 120 and noise about +/-50 of 8388608 (near -104 dBFS), unchanged while a tone plays. A voice has not been recorded: `guest/darwin-sound-listen-headset.in`.
- **User interface for now:** `/dev/pdaudio` (signed 16-bit stereo), `pdplay` (tones, WAVE files), `kern.pd_sound.*`. Output starts 30 dB below the codec's full scale.
- **Not done yet:** system audio (`coreaudiod`), the safety daemon port, capture (I7).
- **Shared services written here because SF-A has none yet** (`m1-sound/kernel/`, reusable as they are): `pd_soc` (register maps by node, PMGR enable with parents and reset), `pd_gpio`, `pd_i2c` (polled), `pd_dart_fixed` (one fixed window on a T8020 DART stream).

| Apple device tree node | Used for |
|---|---|
| `/arm-io/i2c2` (`i2c,t8101`, clock-gate `I2C2`) | codec bus; pins 162/163 of `/arm-io/gpio`, function 1 |
| `/arm-io/i2c2/audio-codec-output` (`audio-control,cs42l83`, reg `0x48`) | codec; reset `/arm-io/nub-gpio` pin 11, interrupt `/arm-io/gpio` pin 183 |
| `/arm-io/nco` (`nco,t8101`, 900 MHz reference) | one clock channel per cluster |
| `/arm-io/mca-switch` (`mca-switch,t8103`; reg 0 clusters, reg 1 DMA switch; clock-gates `MCA0`..`MCA5`, parent `AUDIO_P`) | I2S; cluster 0 drives the port of cluster 2 (the jack), as Linux does |
| `/arm-io/admac-sio` (`admac,t8103`, clock-gates `SIO_ADMA`, `AUDIO-P-V`) | DMA channel 4 x cluster |
| `/arm-io/dart-sio` (`dart,t8020`, clock-gate `SIO-CPU-DART`), child `mapper-admac` (stream 2) | DMA translation |
| `/arm-io/i2c1`, `/arm-io/i2c3` (TAS5770L), `/arm-io/gpio` pin 181 | speakers: not touched |

## Dependencies
SF-A (A2 power domains, A5 DART instance for `dart-sio`, A6 GPIO, A7 I2C; A3 RTKit for I7).

## Interfaces this ticket owns
Audio kexts under `integrations/puredarwin/m1-audio/`; `coreaudiod` capture path.

## Risks
`IOAudioFamily` has never run in our arm64 kernel collection. Clocking and TDM details are easy to get wrong (see Linux `macaudio.c` format constants).

## Research
`docs/roadmap/research/sound-bluetooth.md`, part A.
