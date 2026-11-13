// SPDX-License-Identifier: MIT
// AudioToolbox additions (wrapper over the sound stream's AudioToolbox, re-exported): System
// Sound Services. Nothing here plays yet: there is no system sound server and no alert sound
// file on the system, so a sound cannot be made from a file and the alert sound is silent.
// (An application's bell is then visual only.)
#include <CoreFoundation/CoreFoundation.h>
typedef int32_t OSStatus;
typedef uint32_t SystemSoundID;
enum { kAudioServicesUnsupportedPropertyError = 'pty?', kAudioServicesSystemSoundUnspecifiedError = -1500, kAudioServicesBadSystemSoundIDError = -1501 };
OSStatus AudioServicesCreateSystemSoundID(CFURLRef file, SystemSoundID *outID) { if (outID) *outID = 0; return kAudioServicesSystemSoundUnspecifiedError; }
OSStatus AudioServicesDisposeSystemSoundID(SystemSoundID sound) { return kAudioServicesBadSystemSoundIDError; }
void AudioServicesPlayAlertSound(SystemSoundID sound) { }
OSStatus AudioServicesAddSystemSoundCompletion(SystemSoundID sound, CFRunLoopRef loop, CFStringRef mode, void (*routine)(SystemSoundID, void *), void *clientData) { return kAudioServicesBadSystemSoundIDError; }
void AudioServicesRemoveSystemSoundCompletion(SystemSoundID sound) { }
OSStatus AudioServicesGetProperty(uint32_t property, uint32_t specifierSize, const void *specifier, uint32_t *ioSize, void *outData) { return kAudioServicesUnsupportedPropertyError; }
