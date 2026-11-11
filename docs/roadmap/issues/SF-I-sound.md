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
- **Speakers (boots `sd06`, `sd07`, `sd12`, `sd14`):** the four TAS5770L amplifiers (revision `0x50`, die 25-31 C), the 8-slot TDM stream from cluster 1 to the ports of clusters 0 and 1, and the current/voltage sense capture on cluster 2 run from `pd_tas2770` and `pd_speakers` (ported from Omacom `tas2770.c` and the J293 policy of `macaudio.c`). **The protection model is in the kernel; there is no safety daemon and no interface to unlock anything.** `kernel/pd_speaker_model.c` (MIT) is the voice-coil temperature model of Asahi's speakersafetyd 2.0.1 with its `j293.conf` parameters, in 64-bit fixed point (no floating point in the kernel, and the same object runs in the host tests, bit for bit). Rules, all in `pd_speakers.cpp`: gain register fixed at `0x0f` (18.5 dBV); volume written only by the driver, to what the model allows, and -100 dB when the model is off (`pdspeaker_model=0`) or has faulted; every session starts 20 dB down and is released half a dB per period only after the sensed voltage has followed the played signal and the current fits a speaker; a model fault (coil or magnet above 140 C, mean power below -0.01 W), sense data that stops following the signal or falls more than 4 periods behind, an amplifier fault bit, a register that reads back wrong, an I2C error, a DMA ring error or dry ring, or a DART fault puts the amplifiers into hardware shutdown; after three such stops the speakers stay off until the next boot. State is read-only in `kern.pd_speaker.model`, `.limit_steps`, `.volume_steps`, `.sense_trusted`, `.sense`.
- **Model equivalence (host, `m1-sound/model-test/`):** the kernel model against upstream's unmodified `types.rs`, driven as the daemon's loop drives it: 4662 scenarios (silence, idle noise, sines from 1 mW to full scale, clipping, bursts, reactive and negative power, stuck sense channels, pauses, rate changes, 4500 random) at 44.1 and 48 kHz. At every period end of the reference the kernel model is never cooler (coil +0.05 to +4.29 C, magnet +0.05 to +0.17 C), never sets a higher volume (0 to 37 half-dB steps lower) and never faults later; UBSan/ASan clean; no undefined symbol in the freestanding arm64 object. To hold that for any call pattern the model keeps the daemon's 4096-frame grid (see the header). `./run-model-tests.sh` exits 0 only on that verdict.
- **Sense data on this machine (boot `sd14`):** with the model in charge and the volume held 20 dB down (`kern.pd_speaker.hold_probe=1`), a 440 Hz tone 22 dB below full scale gives mean magnitudes of 139-140 counts on the voltage channels against 138 expected from `vs_scale` (within 1.5 percent) and 57-63 on the current channels, about 8.4 ohm against 9.7 nominal; `sense data follows the signal` on both sides in all five sessions, 48000.03 Hz, no trip except the three forced ones. That tone left the amplifiers at about 65 mV: the only thing the speakers have played. **Nobody has listened**: `guest/darwin-sound-listen-speakers.in`. Unattended scripts keep `hold_probe=1`.
- **Reference daemon (boot `sd08`):** `m1-sound/safetyd/` is the speakersafetyd port; it is no longer run on the machine and serves as the reference of `model-test`.
- **System audio (boots `sd20`, `sd21`, `sd24` to `sd26`):** the tree's `coreaudiod` with a `/dev` backend (`m1-sound/sysaudio/`, see its `DESIGN`): AudioQueue clients, two clients mixed at 44.1 and 48 kHz, volume and mute through CoreAudio mapped to the codec, `pdvolume`; the speakers are the default output while the jack is empty and the headphones when it is not (switch on the driver's jack state); no IOAudioFamily. Recording through `coreaudiod` is not implemented (capture is `/dev/pdmic` only).
- **Whole desktop (boots `sd13`, `sd15`, 8 CPUs):** desktop collection plus sound (`build-kc-desktop.nix`), Wi-Fi joined and pinging, GPU compositor, `coreaudiod`, tones on speakers (held 20 dB down) and headphones, and the volume keys F10/F11/F12 through the compositor (`WMAUTO volume ... result=PASS`).
- **Headset microphone (boot `sd10`):** `/dev/pdmic` captures the codec's converter through the jack port's receiver (cluster 0, second receiver) while the headphone stream runs. With the jack empty: 140 periods in 3 s, both channels the same, offset about 120 and noise about +/-50 of 8388608 (near -104 dBFS), unchanged while a tone plays. A voice has not been recorded: `guest/darwin-sound-listen-headset.in`.
- **Built-in microphones (boots `sd40` to `sd49`, separate kext `m1-sound/aop/`, `PDAopAudio`):** the AOP coprocessor boots under RTKit (protocol 12, 56 ms), its `aop-audio` EPIC service accepts every call Omacom's driver makes, and `/dev/pdmic` of that kext delivers 48 kHz, 3 channels, float32 through `admac-aop-audio` and `dart-aop` with no error (48000.018 Hz). **The samples carry no microphone signal**: an identical start-up transient each session, then numerical zero, two channels bit-identical. The PDM clock pins toggle; the pins read as PDM data stay high. Not known whether Linux gets live samples on this unit: a person must record there (`arecord -D hw:AppleJ293HPAI,0 -f FLOAT_LE -c 3 -r 48000 -d 3`) and run `aop/guest/darwin-mic-listen.in` with the lid open.
- **User interface for now:** `/dev/pdaudio` (signed 16-bit stereo), `pdplay` (tones, WAVE files), `kern.pd_sound.*`. Output starts 30 dB below the codec's full scale.
- **Not done yet:** recording through `coreaudiod`; internal microphones (AOP), see `m1-sound/aop/`.
- **Shared services written here because SF-A has none yet** (`m1-sound/kernel/`, reusable as they are): `pd_soc` (register maps by node, PMGR enable with parents and reset), `pd_gpio`, `pd_i2c` (polled), `pd_dart_fixed` (one fixed window on a T8020 DART stream).

| Apple device tree node | Used for |
|---|---|
| `/arm-io/i2c2` (`i2c,t8101`, clock-gate `I2C2`) | codec bus; pins 162/163 of `/arm-io/gpio`, function 1 |
| `/arm-io/i2c2/audio-codec-output` (`audio-control,cs42l83`, reg `0x48`) | codec; reset `/arm-io/nub-gpio` pin 11, interrupt `/arm-io/gpio` pin 183 |
| `/arm-io/nco` (`nco,t8101`, 900 MHz reference) | one clock channel per cluster |
| `/arm-io/mca-switch` (`mca-switch,t8103`; reg 0 clusters, reg 1 DMA switch; clock-gates `MCA0`..`MCA5`, parent `AUDIO_P`) | I2S; cluster 0 drives the port of cluster 2 (the jack), as Linux does |
| `/arm-io/admac-sio` (`admac,t8103`, clock-gates `SIO_ADMA`, `AUDIO-P-V`) | DMA channel 4 x cluster |
| `/arm-io/dart-sio` (`dart,t8020`, clock-gate `SIO-CPU-DART`), child `mapper-admac` (stream 2) | DMA translation |
| `/arm-io/i2c1`, `/arm-io/i2c3` (TAS5770L at 0x31/0x32 and 0x34/0x35), `/arm-io/gpio` pin 181 (shutdown) | speakers; front end cluster 1, sense capture cluster 2 |

## Dependencies
SF-A (A2 power domains, A5 DART instance for `dart-sio`, A6 GPIO, A7 I2C; A3 RTKit for I7).

## Interfaces this ticket owns
Audio kexts under `integrations/puredarwin/m1-audio/`; `coreaudiod` capture path.

## Risks
`IOAudioFamily` has never run in our arm64 kernel collection. Clocking and TDM details are easy to get wrong (see Linux `macaudio.c` format constants).

## Research
`docs/roadmap/research/sound-bluetooth.md`, part A.
