#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read only selected live AVE ADT metadata; no MMIO mapping or image bytes."""
import ctypes as C
import json
import os
import struct


def main():
    if os.uname().sysname!='Darwin':raise SystemExit('Run on the M1 SiliconForest root')
    kit=C.CDLL('/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit')
    kit.IORegistryEntryFromPath.argtypes=[C.c_uint,C.c_char_p];kit.IORegistryEntryFromPath.restype=C.c_uint
    kit.IORegistryEntryGetProperty.argtypes=[C.c_uint,C.c_char_p,C.c_void_p,C.POINTER(C.c_uint)];kit.IORegistryEntryGetProperty.restype=C.c_int
    kit.IOObjectRelease.argtypes=[C.c_uint]
    rows=[]
    for path in ['IODeviceTree:/arm-io/ave','IODeviceTree:/chosen']:
        entry=kit.IORegistryEntryFromPath(0,path.encode())
        item={'path':path,'entry_found':bool(entry),'properties':{}}
        try:
            if entry:
                keys=['name','compatible','pre-loaded','firmware-name','firmware-version','system-firmware-version',
                      'os-firmware-version','os-version','os-build-version','firmware-version-string','segment-ranges','segment-names']
                for key in keys:
                    buf=C.create_string_buffer(65536);length=C.c_uint(len(buf))
                    rc=kit.IORegistryEntryGetProperty(entry,key.encode(),buf,C.byref(length))
                    if rc:continue
                    if length.value>len(buf):raise ValueError('Registry output exceeds bound')
                    value=buf.raw[:length.value]
                    if key=='segment-ranges':
                        if len(value)%32 or len(value)>4*32:raise ValueError('Invalid segment-ranges')
                        item['properties'][key]={'byte_length':len(value),'segments':[
                            dict(zip(['physical','iova','remap','size','flags'],t)) for t in struct.iter_unpack('<QQQII',value)]}
                    elif key=='pre-loaded':item['properties'][key]=int.from_bytes(value,'little')
                    else:
                        # Unknown binary properties never leave the process.
                        try:item['properties'][key]=value.rstrip(b'\0').decode('ascii')
                        except UnicodeDecodeError:item['properties'][key]={'byte_length':len(value),'not_printed':True}
        finally:
            if entry:kit.IOObjectRelease(entry)
        rows.append(item)
    print(json.dumps(rows,indent=2))


if __name__=='__main__':main()
