/* SPDX-License-Identifier: MIT */
/* Our bounded local playback engine: AVAssetReader -> actual VideoToolbox,
 * decoded NV12 -> CPU image, ExtAudioFile -> retained AudioQueue output.
 * Preparing a file is bounded and synchronous; this is not a streaming engine.
 */
#import <AVFoundation/AVFoundation.h>
#import "SFPlayerMedia.h"
#include <AudioToolbox/AudioQueue.h>
#include "VideoToolbox.h"
#include <libswscale/swscale.h>
#include <libavutil/md5.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

typedef struct { CMTime pts; size_t width,height; uint8_t *bytes; char md5[33]; } SFMediaFrame;
@interface SFPlayerMedia () {
@public
 pthread_mutex_t _lock;
 SFMediaFrame *_frames;size_t _frameCount,_frameCapacity,_frameBytes;
 uint8_t *_pcm;size_t _audioBytes,_audioNext,_audioReturned,_audioStart;
 AudioStreamBasicDescription _format;AudioQueueRef _queue;
 double _baseSeconds,_deviceRate;BOOL _running,_closing,_hardware;
 _Atomic(OSStatus) _error;unsigned _callbacks;size_t _submitted;
 struct SwsContext *_scale;CGImageRef _cachedImage;size_t _cachedIndex;
}
- (void)closeQueue;
@end
static void sfKeepStatus(SFPlayerMedia *s,OSStatus status){if(status)s->_error=status;}
static NSError *sfError(OSStatus status,NSString *operation){return [NSError errorWithDomain:@"SiliconForestPlayer" code:status?:-1 userInfo:@{NSLocalizedDescriptionKey:operation}];}
static int sfOrder(const void *left,const void *right){return CMTimeCompare(((const SFMediaFrame *)left)->pts,((const SFMediaFrame *)right)->pts);}
static void sfDecoded(void *context,void *source,OSStatus status,VTDecodeInfoFlags flags,CVImageBufferRef image,CMTime pts,CMTime duration){
 (void)source;(void)flags;(void)duration;SFPlayerMedia *s=(__bridge SFPlayerMedia *)context;
 pthread_mutex_lock(&s->_lock);
 if(status||!image||!isfinite(CMTimeGetSeconds(pts))||s->_frameCount>=4096){s->_error=status?:-1;goto out;}
 size_t w=CVPixelBufferGetWidth(image),h=CVPixelBufferGetHeight(image);
 if(!w||!h||w>4096||h>2160||(w&1)||(h&1)||CVPixelBufferGetPixelFormatType(image)!=kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange||CVPixelBufferGetPlaneCount(image)!=2){s->_error=-1;goto out;}
 size_t size=w*h*3/2;
 if(size>512u*1024u*1024u-s->_frameBytes){s->_error=-1;goto out;}
 uint8_t *data=malloc(size);if(!data){s->_error=-1;goto out;}
 if(CVPixelBufferLockBaseAddress(image,kCVPixelBufferLock_ReadOnly)){free(data);s->_error=-1;goto out;}
 const uint8_t *y=CVPixelBufferGetBaseAddressOfPlane(image,0),*uv=CVPixelBufferGetBaseAddressOfPlane(image,1);
 size_t ys=CVPixelBufferGetBytesPerRowOfPlane(image,0),us=CVPixelBufferGetBytesPerRowOfPlane(image,1);
 if(!y||!uv||ys<w||us<w){CVPixelBufferUnlockBaseAddress(image,kCVPixelBufferLock_ReadOnly);free(data);s->_error=-1;goto out;}
 for(size_t n=0;n<h;n++)memcpy(data+n*w,y+n*ys,w);
 for(size_t n=0;n<h/2;n++)memcpy(data+w*h+n*w,uv+n*us,w);
 CVPixelBufferUnlockBaseAddress(image,kCVPixelBufferLock_ReadOnly);
 if(s->_frameCount==s->_frameCapacity){size_t capacity=s->_frameCapacity?s->_frameCapacity*2:64;void *next=realloc(s->_frames,capacity*sizeof(SFMediaFrame));if(!next){free(data);s->_error=-1;goto out;}s->_frames=next;s->_frameCapacity=capacity;}
 SFMediaFrame *frame=&s->_frames[s->_frameCount++];*frame=(SFMediaFrame){.pts=pts,.width=w,.height=h,.bytes=data};
 uint8_t digest[16];av_md5_sum(digest,data,size);for(unsigned n=0;n<16;n++)snprintf(frame->md5+2*n,3,"%02x",digest[n]);s->_frameBytes+=size;
 out:pthread_mutex_unlock(&s->_lock);
}
static void sfFeed(SFPlayerMedia *s,AudioQueueRef queue,AudioQueueBufferRef buffer){
 if(s->_closing||s->_error||s->_audioNext>=s->_audioBytes)return;
 size_t count=s->_audioBytes-s->_audioNext;if(count>buffer->mAudioDataBytesCapacity)count=buffer->mAudioDataBytesCapacity;
 memcpy(buffer->mAudioData,s->_pcm+s->_audioNext,count);buffer->mAudioDataByteSize=(UInt32)count;
 OSStatus status=AudioQueueEnqueueBuffer(queue,buffer,0,NULL);if(status)s->_error=status;else s->_audioNext+=count;
}
static void sfReturned(void *context,AudioQueueRef queue,AudioQueueBufferRef buffer){
 SFPlayerMedia *s=(__bridge SFPlayerMedia *)context;pthread_mutex_lock(&s->_lock);
 if(!s->_closing){s->_audioReturned+=buffer->mAudioDataByteSize;s->_callbacks++;sfFeed(s,queue,buffer);}
 pthread_mutex_unlock(&s->_lock);
}
static void sfFreeImage(void *info,const void *bytes,size_t size){(void)info;(void)size;free((void *)bytes);}
@implementation SFPlayerMedia
+ (BOOL)needsBackend:(AVAsset *)asset {
 AVAssetTrack *video=[asset tracksWithMediaType:AVMediaTypeVideo].firstObject;
 if([asset tracksWithMediaType:AVMediaTypeAudio].count)return YES;
 CMFormatDescriptionRef description=video.formatDescriptions.count?(__bridge CMFormatDescriptionRef)video.formatDescriptions.firstObject:NULL;
 OSType codec=description?CMFormatDescriptionGetMediaSubType(description):0;
 return codec==kCMVideoCodecType_H264||codec==kCMVideoCodecType_HEVC||codec==0x76703039;
}
- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)error {
 if(!(self=[super init]))return nil;pthread_mutex_init(&_lock,NULL);atomic_init(&_error,0);_cachedIndex=SIZE_MAX;
 NSURL *url=[asset respondsToSelector:@selector(URL)]?[(AVURLAsset *)asset URL]:nil;
 if(!url.isFileURL){if(error)*error=sfError(-1,@"Only bounded local files are supported");return nil;}
 AVAssetTrack *track=[asset tracksWithMediaType:AVMediaTypeVideo].firstObject;
 if(track){
  NSError *readError=nil;AVAssetReader *reader=[[AVAssetReader alloc] initWithAsset:asset error:&readError];
  AVAssetReaderTrackOutput *output=[[AVAssetReaderTrackOutput alloc] initWithTrack:track outputSettings:nil];
  if(!reader||![reader canAddOutput:output]){if(error)*error=readError?:sfError(-1,@"Cannot read video track");return nil;}
  [reader addOutput:output];if(![reader startReading]){if(error)*error=reader.error?:sfError(-1,@"Cannot start video reader");return nil;}
  VTDecompressionSessionRef session=NULL;CMSampleBufferRef sample;
  while((sample=[output copyNextSampleBuffer])){
   if(!CMSampleBufferGetDataBuffer(sample)&&!CMSampleBufferGetNumSamples(sample)){CFRelease(sample);continue;}
   if(!session){
    int32_t pixel=kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;CFNumberRef number=CFNumberCreate(NULL,kCFNumberSInt32Type,&pixel);
    const void *keys[]={kCVPixelBufferPixelFormatTypeKey};const void *values[]={number};
    CFDictionaryRef attrs=CFDictionaryCreate(NULL,keys,values,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    VTDecompressionOutputCallbackRecord callback={sfDecoded,(__bridge void *)self};
    _error=VTDecompressionSessionCreate(NULL,CMSampleBufferGetFormatDescription(sample),NULL,attrs,&callback,&session);CFRelease(number);CFRelease(attrs);
   }
   if(!_error)sfKeepStatus(self,VTDecompressionSessionDecodeFrame(session,sample,0,NULL,NULL));CFRelease(sample);_submitted++;
   if(_error||_submitted>4096)break;
  }
  if(session){
   if(!_error)sfKeepStatus(self,VTDecompressionSessionFinishDelayedFrames(session));if(!_error)sfKeepStatus(self,VTDecompressionSessionWaitForAsynchronousFrames(session));
   CFTypeRef hardware=NULL;if(!VTSessionCopyProperty(session,kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,NULL,&hardware)){_hardware=hardware==kCFBooleanTrue;if(hardware)CFRelease(hardware);}
   VTDecompressionSessionInvalidate(session);CFRelease(session);
  }
  if(_error||reader.status!=AVAssetReaderStatusCompleted||!_frameCount||_frameCount!=_submitted){if(error)*error=reader.error?:sfError(_error,@"Video decode failed or exceeds bounded local-file storage");return nil;}
  qsort(_frames,_frameCount,sizeof(SFMediaFrame),sfOrder);
  for(size_t n=1;n<_frameCount;n++)if(CMTimeCompare(_frames[n-1].pts,_frames[n].pts)>=0){if(error)*error=sfError(-1,@"Video timestamps are not strictly ordered");return nil;}
 }
 if([asset tracksWithMediaType:AVMediaTypeAudio].count){
  ExtAudioFileRef file=NULL;_error=ExtAudioFileOpenURL((__bridge CFURLRef)url,&file);
  if(!_error){UInt32 bytes=sizeof(_format);_error=ExtAudioFileGetProperty(file,kExtAudioFileProperty_FileDataFormat,&bytes,&_format);}
  if(!_error&&(!_format.mChannelsPerFrame||_format.mChannelsPerFrame>2||!isfinite(_format.mSampleRate)||_format.mSampleRate<8000||_format.mSampleRate>192000))_error=-1;
  if(!_error){UInt32 channels=_format.mChannelsPerFrame;double rate=_format.mSampleRate;_format=(AudioStreamBasicDescription){rate,kAudioFormatLinearPCM,kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked,channels*2,1,channels*2,channels,16,0};_error=ExtAudioFileSetProperty(file,kExtAudioFileProperty_ClientDataFormat,sizeof(_format),&_format);}
  while(!_error){uint8_t buffer[16384];UInt32 frames=sizeof(buffer)/_format.mBytesPerFrame;AudioBufferList list={1,{{_format.mChannelsPerFrame,sizeof(buffer),buffer}}};_error=ExtAudioFileRead(file,&frames,&list);if(_error||!frames)break;if(frames>sizeof(buffer)/_format.mBytesPerFrame){_error=-1;break;}
   size_t count=(size_t)frames*_format.mBytesPerFrame;if(count>64u*1024u*1024u-_audioBytes){_error=-1;break;}void *next=realloc(_pcm,_audioBytes+count);if(!next){_error=-1;break;}_pcm=next;memcpy(_pcm+_audioBytes,buffer,count);_audioBytes+=count;
  }
  if(file)ExtAudioFileDispose(file);if(_error||!_audioBytes){if(error)*error=sfError(_error,@"Audio decode failed or exceeds bounded local-file storage");return nil;}
 }
 return self;
}
- (void)dealloc { [self closeQueue];for(size_t n=0;n<_frameCount;n++)free(_frames[n].bytes);free(_frames);free(_pcm);if(_cachedImage)CGImageRelease(_cachedImage);sws_freeContext(_scale);pthread_mutex_destroy(&_lock); }
- (BOOL)hasAudio {return _audioBytes!=0;}
- (BOOL)isAudioDrained {pthread_mutex_lock(&_lock);BOOL result=!_audioBytes||(_queue&&_audioReturned>=_audioBytes-_audioStart);pthread_mutex_unlock(&_lock);return result;}
- (NSError *)playbackError {return _error?sfError(_error,@"Retained audio output failed"):nil;}
- (void)closeQueue {
 pthread_mutex_lock(&_lock);_closing=YES;AudioQueueRef queue=_queue;pthread_mutex_unlock(&_lock);
 if(queue){AudioQueueStop(queue,true);AudioQueueDispose(queue,true);}
 pthread_mutex_lock(&_lock);_queue=NULL;_running=NO;_closing=NO;pthread_mutex_unlock(&_lock);
}
- (BOOL)startAtSeconds:(double)seconds volume:(float)volume {
 if(!_audioBytes)return YES;if(!isfinite(seconds)||seconds<0)return NO;
 if(_queue&&fabs([self clockSeconds]-seconds)>.002)[self closeQueue];
 if(!_queue){
  _baseSeconds=seconds;size_t frame=(size_t)floor(seconds*_format.mSampleRate);size_t total=_audioBytes/_format.mBytesPerFrame;
  if(frame>total)frame=total;_audioStart=frame*_format.mBytesPerFrame;_audioNext=_audioStart;_audioReturned=0;_callbacks=0;_error=0;
  _error=AudioQueueNewOutput(&_format,sfReturned,(__bridge void *)self,NULL,NULL,0,&_queue);if(_error)return NO;
  UInt32 size=sizeof(_deviceRate);_error=AudioQueueGetProperty(_queue,kAudioQueueDeviceProperty_SampleRate,&_deviceRate,&size);
  if(_error||!isfinite(_deviceRate)||_deviceRate<=0){if(!_error)_error=-1;[self closeQueue];return NO;}
  for(unsigned n=0;n<3&&!_error;n++){AudioQueueBufferRef buffer=NULL;_error=AudioQueueAllocateBuffer(_queue,1024*_format.mBytesPerFrame,&buffer);if(!_error)sfFeed(self,_queue,buffer);}
 }
 if(!_error)_error=AudioQueueSetParameter(_queue,kAudioQueueParam_Volume,volume);
 if(!_error)sfKeepStatus(self,AudioQueueStart(_queue,NULL));_running=!_error;return _running;
}
- (void)pause {if(_queue){OSStatus status=AudioQueuePause(_queue);if(status)_error=status;}_running=NO;}
- (void)setVolume:(float)volume {if(_queue){OSStatus status=AudioQueueSetParameter(_queue,kAudioQueueParam_Volume,volume);if(status)_error=status;}}
- (double)clockSeconds {if(!_queue||!_deviceRate)return NAN;AudioTimeStamp time={0};OSStatus status=AudioQueueGetCurrentTime(_queue,NULL,&time,NULL);if(status){_error=status;return NAN;}double seconds=_baseSeconds+time.mSampleTime/_deviceRate;if([self isAudioDrained])seconds=MAX(seconds,(double)(_audioBytes/_format.mBytesPerFrame)/_format.mSampleRate);return seconds;}
- (CGImageRef)copyImageAtTime:(CMTime)time index:(size_t *)outIndex {
 if(!_frameCount||!CMTIME_IS_NUMERIC(time))return NULL;
 size_t index=0;for(size_t n=1;n<_frameCount&&CMTimeCompare(_frames[n].pts,time)<=0;n++)index=n;
 if(outIndex)*outIndex=index;if(index==_cachedIndex&&_cachedImage)return CGImageRetain(_cachedImage);
 SFMediaFrame *frame=&_frames[index];int w=(int)frame->width,h=(int)frame->height;
 uint8_t *pixels=malloc((size_t)w*h*4);if(!pixels)return NULL;
 _scale=sws_getCachedContext(_scale,w,h,AV_PIX_FMT_NV12,w,h,AV_PIX_FMT_RGBA,SWS_BILINEAR,NULL,NULL,NULL);if(!_scale){free(pixels);return NULL;}
 const uint8_t *src[4]={frame->bytes,frame->bytes+w*h,NULL,NULL};int strides[4]={w,w,0,0};uint8_t *dst[4]={pixels,NULL,NULL,NULL};int output[4]={w*4,0,0,0};
 if(sws_scale(_scale,src,strides,0,h,dst,output)!=h){free(pixels);return NULL;}
 CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,pixels,(size_t)w*h*4,sfFreeImage);CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
 CGImageRef image=provider&&color?CGImageCreate(w,h,8,32,w*4,color,kCGImageAlphaNoneSkipLast|kCGBitmapByteOrderDefault,provider,NULL,false,kCGRenderingIntentDefault):NULL;
 if(provider)CGDataProviderRelease(provider);else free(pixels);if(color)CGColorSpaceRelease(color);
 if(!image)return NULL;if(_cachedImage)CGImageRelease(_cachedImage);_cachedImage=image;_cachedIndex=index;return CGImageRetain(image);
}
- (NSArray *)frameDigests {NSMutableArray *result=[NSMutableArray arrayWithCapacity:_frameCount];for(size_t n=0;n<_frameCount;n++)[result addObject:@{@"pts":@(CMTimeGetSeconds(_frames[n].pts)),@"md5":[NSString stringWithUTF8String:_frames[n].md5]}];return result;}
- (NSDictionary *)statistics {
 double clock=[self clockSeconds];pthread_mutex_lock(&_lock);
 NSDictionary *result=@{@"videoFrames":@(_frameCount),@"videoSamples":@(_submitted),@"hardware":@(_hardware),@"audioFrames":@(_format.mBytesPerFrame?_audioBytes/_format.mBytesPerFrame:0),@"audioQueued":@(_format.mBytesPerFrame?_audioNext/_format.mBytesPerFrame:0),@"audioReturned":@(_format.mBytesPerFrame?_audioReturned/_format.mBytesPerFrame:0),@"callbacks":@(_callbacks),@"clientRate":@(_format.mSampleRate),@"deviceRate":@(_deviceRate),@"clock":@(isfinite(clock)?clock:0),@"error":@(_error),@"frameBytes":@(_frameBytes)};
 pthread_mutex_unlock(&_lock);return result;
}
@end
