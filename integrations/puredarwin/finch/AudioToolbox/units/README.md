# Retained PD AUHAL real device selection (prefix candidate)

VLC's actual auhal plugin selects the real default output ID272. The retained
PD AudioUnit rejected every ID except its0x40 follow-default alias, returning
-10851 before initialization. patch-unit.py ports that implementation from a
fresh PD source copy: each unit retains its selected validated alive output ID,
queries its output PCM format/buffer size and uses it for IOProc lifecycle.
The default alias remains supported; unknown IDs and changes after initialize
are rejected. Converter/queue/unit opaque objects stay in one retained image.

build.nix builds PD's family with this one AudioUnit source patch. Its generated
CoreAudio is NEVER staged or installed: the installed Root CoreAudio/HAL and
coreaudiod remain the transport. Prefix qualification copies only the private
AudioToolbox.output-pd into our existing Finch framework; its78 reviewed exports
are all retained (80total). Canonical files have not changed for this candidate.

probe.c selects the real default output, rejects an unknown ID, registers its
own mono48k PCM callback and runs the public AUHAL initialize/start/stop/dispose
lifecycle. M5 reference of the same Linux-built executable passes24callbacks/
12288produced; native clock values are device-global and not claimed as this
process's consumed ring. On our Root AudioDeviceGetCurrentTime reports the
actual per-process readFrame consumed by coreaudiod; M1 prefix test passes28callbacks/14339produced and12288 HAL-consumed
frames, selected actual default ID and rejected unknown, stop/uninit/dispose0.
No acoustic or microphone claim. Evidence: runs/sffinch/au-device-stage-v1 and
m5-au-device-v1-reference.json.
