/* SPDX-License-Identifier: MIT */
/* Own silent units only. Observe the actual HAL -> AU callback timestamp
 * boundary without changing output timing, volume, device or PCM content. */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAPACITY 1024
struct io_context { AudioDeviceIOProc proc; void *data; };
static struct io_context contexts[16];
static unsigned context_count;
static _Thread_local const AudioTimeStamp *current_output;
static _Thread_local uint64_t current_entry;
static _Thread_local double current_client_base;
static _Thread_local unsigned current_callback;
struct row {
    uint64_t now, entry, hal_host, au_host;
    double hal_sample, au_sample, client_base;
    unsigned hal_flags, au_flags, frames, cycle_callback, case_id;
};
static struct row rows[CAPACITY];
static unsigned row_count, current_case;
static OSStatus io_proxy(AudioObjectID device,const AudioTimeStamp *now,
        const AudioBufferList *input,const AudioTimeStamp *input_time,
        AudioBufferList *output,const AudioTimeStamp *output_time,void *opaque)
{
    struct io_context *context=opaque;
    const AudioTimeStamp *previous=current_output;
    current_output=output_time; current_entry=mach_absolute_time(); current_callback=0;
    OSStatus result=context->proc(device,now,input,input_time,output,output_time,context->data);
    current_output=previous;
    return result;
}
static OSStatus trace_create(AudioObjectID device,AudioDeviceIOProc proc,void *data,AudioDeviceIOProcID *out)
{
    if (!proc || context_count==16) return AudioDeviceCreateIOProcID(device,proc,data,out);
    struct io_context *context=&contexts[context_count++];
    *context=(struct io_context){proc,data};
    return AudioDeviceCreateIOProcID(device,io_proxy,context,out);
}
/* dyld excludes this image's original references from its own interposition. */
__attribute__((used,section("__DATA,__interpose")))
static const struct { const void *replacement,*original; } interpose={trace_create,AudioDeviceCreateIOProcID};

static void need(int good,const char *what)
{ if (!good) { fprintf(stderr,"LATENCY_PROBE_FAIL %s\n",what); exit(1); } }
static OSStatus render(void *opaque,AudioUnitRenderActionFlags *flags,const AudioTimeStamp *stamp,
        UInt32 bus,UInt32 frames,AudioBufferList *data)
{
    (void)opaque; (void)flags; (void)bus;
    uint64_t now=mach_absolute_time();
    unsigned index=__atomic_fetch_add(&row_count,1,__ATOMIC_RELAXED);
    if (current_callback==0) current_client_base=stamp->mSampleTime;
    if (index<CAPACITY) rows[index]=(struct row){
        .now=now,.entry=current_entry,.hal_host=current_output?current_output->mHostTime:0,
        .au_host=stamp->mHostTime,.hal_sample=current_output?current_output->mSampleTime:0,
        .au_sample=stamp->mSampleTime,.client_base=current_client_base,
        .hal_flags=current_output?current_output->mFlags:0,.au_flags=stamp->mFlags,
        .frames=frames,.cycle_callback=current_callback,.case_id=current_case};
    current_callback++;
    for (UInt32 b=0;b<data->mNumberBuffers;b++)
        memset(data->mBuffers[b].mData,0,data->mBuffers[b].mDataByteSize);
    return noErr;
}
static UInt32 property(AudioObjectID object,AudioObjectPropertySelector selector,AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress a={selector,scope,kAudioObjectPropertyElementMain};
    UInt32 value=0,size=sizeof(value);
    need(AudioObjectGetPropertyData(object,&a,0,NULL,&size,&value)==0&&size==sizeof(value),"device latency property");
    return value;
}
static void run_case(AudioDeviceID device,double rate,double device_rate,unsigned case_id)
{
    AudioComponentDescription desc={kAudioUnitType_Output,kAudioUnitSubType_HALOutput,kAudioUnitManufacturer_Apple,0,0};
    AudioComponent component=AudioComponentFindNext(NULL,&desc); AudioUnit unit=NULL;
    need(component&&AudioComponentInstanceNew(component,&unit)==0&&unit,"new own output");
    need(AudioUnitSetProperty(unit,kAudioOutputUnitProperty_CurrentDevice,kAudioUnitScope_Global,0,&device,sizeof(device))==0,"select output");
    AudioStreamBasicDescription format={rate,kAudioFormatLinearPCM,kAudioFormatFlagsNativeFloatPacked,8,1,8,2,32,0};
    AURenderCallbackStruct callback={render,NULL};
    need(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format))==0,"set client format");
    need(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback))==0,"set silent callback");
    need(AudioUnitInitialize(unit)==0,"initialize");
    Float64 unit_latency=0; UInt32 size=sizeof(unit_latency);
    need(AudioUnitGetProperty(unit,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&unit_latency,&size)==0&&size==sizeof(unit_latency),"unit latency");
    AudioStreamID streams[32]={0};size=sizeof(streams);
    AudioObjectPropertyAddress a={kAudioDevicePropertyStreams,kAudioObjectPropertyScopeOutput,kAudioObjectPropertyElementMain};
    need(AudioObjectGetPropertyData(device,&a,0,NULL,&size,streams)==0&&size>=sizeof(AudioStreamID)&&size<=sizeof(streams),"output streams");
    UInt32 stream_latency=property(streams[0],kAudioStreamPropertyLatency,kAudioObjectPropertyScopeGlobal);
    printf("LATENCY_CASE case=%u client-rate=%.17g device-rate=%.17g device-latency=%u stream-latency=%u unit-latency=%.17g\n",
           case_id,rate,device_rate,property(device,kAudioDevicePropertyLatency,kAudioObjectPropertyScopeOutput),stream_latency,unit_latency);
    current_case=case_id;
    need(AudioOutputUnitStart(unit)==0,"start own silence"); usleep(800000);
    need(AudioOutputUnitStop(unit)==0,"stop own silence");
    need(AudioUnitUninitialize(unit)==0&&AudioComponentInstanceDispose(unit)==0,"dispose own unit");
}
int main(void)
{
    setvbuf(stdout,NULL,_IOLBF,0);
    if (access("/dev/pdavd",F_OK)) { fprintf(stderr,"M1-only silent timing probe; no output on reference Mac\n"); return 77; }
    AudioObjectPropertyAddress a={kAudioHardwarePropertyDefaultOutputDevice,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
    AudioDeviceID device=0;UInt32 size=sizeof(device);
    need(AudioObjectGetPropertyData(kAudioObjectSystemObject,&a,0,NULL,&size,&device)==0&&device,"default device");
    Float64 rate=0;size=sizeof(rate);a.mSelector=kAudioDevicePropertyNominalSampleRate;
    need(AudioObjectGetPropertyData(device,&a,0,NULL,&size,&rate)==0&&rate>=8000,"device rate");
    mach_timebase_info_data_t tb={0};need(mach_timebase_info(&tb)==0&&tb.numer&&tb.denom,"timebase");
    printf("LATENCY_TIMEBASE numer=%u denom=%u no-acoustic-claim=1\n",tb.numer,tb.denom);
    run_case(device,48000,rate,0); run_case(device,44100,rate,1);
    need(row_count>0&&row_count<=CAPACITY,"bounded real callback records");
    for (unsigned i=0;i<row_count;i++) {
        const struct row *r=&rows[i];
        printf("LATENCY_ROW case=%u now=%llu entry=%llu hal-host=%llu au-host=%llu hal-sample=%.17g au-sample=%.17g client-base=%.17g hal-flags=%u au-flags=%u frames=%u cycle-callback=%u\n",
            r->case_id,(unsigned long long)r->now,(unsigned long long)r->entry,(unsigned long long)r->hal_host,(unsigned long long)r->au_host,
            r->hal_sample,r->au_sample,r->client_base,r->hal_flags,r->au_flags,r->frames,r->cycle_callback);
        need((r->hal_flags&kAudioTimeStampHostTimeValid)!=0,"real HAL callback interposition reached");
    }
    printf("LATENCY_DONE rows=%u io-contexts=%u no-timestamp-change=1 no-acoustic-claim=1\n",row_count,context_count);
    return 0;
}
