/* SPDX-License-Identifier: MIT */
/*
 * pdtone: a sine tone through an AudioQueue, for checking the path from AudioToolbox through
 * coreaudiod to the sound driver without anyone listening.
 *
 *   pdtone [-r rate] [-f hertz] [-s seconds] [-d dB below full scale] [-c l|r|b] [-n name]
 *
 * The queue is given signed 16-bit stereo at the rate asked for (default 48000 Hz, 440 Hz,
 * 3 s, 20 dB below full scale, both channels); AudioToolbox converts it to the device's
 * format. At the end it reports how many buffers came back from the queue and at which rate
 * the queue took the frames, measured from the callbacks after the first two seconds (the
 * start of the output device is not steady): that rate is the device's clock as the client
 * sees it.
 */
#include <AudioToolbox/AudioToolbox.h>
#include <mach/mach_time.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFERS 3

struct tone {
    const char *name;
    double rate, hertz, amplitude, phase;
    int left, right;
    long total, made;           /* frames to make, made so far */
    UInt32 bufferFrames;
    int outstanding, callbacks;
    long returned;              /* frames in the buffers that came back */
    double start, firstAt, lastAt;
    long firstReturned;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

static double
seconds(void)
{
    static mach_timebase_info_data_t tb;

    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom / 1e9;
}

/* fills and queues the buffer, or lets it go when the tone is complete */
static void
fill(struct tone *t, AudioQueueRef q, AudioQueueBufferRef b)
{
    int16_t *out = b->mAudioData;
    long n = t->total - t->made;

    if (n > (long)t->bufferFrames)
        n = t->bufferFrames;
    for (long i = 0; i < n; i++) {
        int16_t s = (int16_t)(t->amplitude * sin(t->phase));

        t->phase += 2 * M_PI * t->hertz / t->rate;
        if (t->phase > 2 * M_PI)
            t->phase -= 2 * M_PI;
        out[2 * i] = t->left ? s : 0;
        out[2 * i + 1] = t->right ? s : 0;
    }
    t->made += n;
    b->mAudioDataByteSize = (UInt32)n * 4;
    if (n == 0 || AudioQueueEnqueueBuffer(q, b, 0, NULL) != noErr) {
        pthread_mutex_lock(&t->lock);
        t->outstanding--;
        pthread_cond_signal(&t->cond);
        pthread_mutex_unlock(&t->lock);
    }
}

/* on the queue's thread, each time a buffer has been taken */
static void
played(void *user, AudioQueueRef q, AudioQueueBufferRef b)
{
    struct tone *t = user;
    double now = seconds();

    t->callbacks++;
    t->returned += b->mAudioDataByteSize / 4;
    if (now - t->start >= 2.0 && t->firstAt == 0) {
        t->firstAt = now;
        t->firstReturned = t->returned;
    }
    t->lastAt = now;
    fill(t, q, b);
}

int
main(int argc, char **argv)
{
    struct tone t = { .name = "tone", .rate = 48000, .hertz = 440, .left = 1, .right = 1 };
    double length = 3, db = 20;
    AudioStreamBasicDescription fmt = { 0 };
    AudioQueueRef q = NULL;
    Float64 deviceRate = 0;
    UInt32 size = sizeof(deviceRate);
    OSStatus err;
    int c;

    while ((c = getopt(argc, argv, "r:f:s:d:c:n:")) != -1) {
        switch (c) {
        case 'r': t.rate = atof(optarg); break;
        case 'f': t.hertz = atof(optarg); break;
        case 's': length = atof(optarg); break;
        case 'd': db = atof(optarg); break;
        case 'c': t.left = optarg[0] != 'r'; t.right = optarg[0] != 'l'; break;
        case 'n': t.name = optarg; break;
        default:
            fprintf(stderr, "usage: pdtone [-r rate] [-f hertz] [-s seconds] [-d dB below full scale] [-c l|r|b] [-n name]\n");
            return 2;
        }
    }
    if (t.rate < 8000 || t.rate > 192000 || length <= 0 || length > 600 || db < 6) {
        fprintf(stderr, "pdtone: rate 8000..192000, up to 600 s, at least 6 dB below full scale\n");
        return 2;
    }
    t.amplitude = 32767.0 * pow(10.0, -db / 20.0);
    t.total = (long)(length * t.rate);
    t.bufferFrames = (UInt32)(t.rate / 10);
    pthread_mutex_init(&t.lock, NULL);
    pthread_cond_init(&t.cond, NULL);

    fmt.mSampleRate = t.rate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame = 4;
    fmt.mFramesPerPacket = 1;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 16;
    if ((err = AudioQueueNewOutput(&fmt, played, &t, NULL, NULL, 0, &q)) != noErr) {
        fprintf(stderr, "pdtone[%s]: AudioQueueNewOutput failed (%d): is coreaudiod running with an output device?\n",
                t.name, (int)err);
        return 1;
    }
    AudioQueueGetProperty(q, kAudioQueueDeviceProperty_SampleRate, &deviceRate, &size);
    for (int i = 0; i < BUFFERS; i++) {
        AudioQueueBufferRef b = NULL;

        if (AudioQueueAllocateBuffer(q, t.bufferFrames * 4, &b) != noErr)
            break;
        pthread_mutex_lock(&t.lock);
        t.outstanding++;
        pthread_mutex_unlock(&t.lock);
        fill(&t, q, b);
    }
    t.start = seconds();
    if ((err = AudioQueueStart(q, NULL)) != noErr) {
        fprintf(stderr, "pdtone[%s]: AudioQueueStart failed (%d)\n", t.name, (int)err);
        return 1;
    }
    fprintf(stderr, "pdtone[%s]: %.0f Hz at %.0f Hz for %.1f s, %.0f dB below full scale; the device runs at %.0f Hz\n",
            t.name, t.hertz, t.rate, length, db, deviceRate);
    pthread_mutex_lock(&t.lock);
    while (t.outstanding > 0)
        pthread_cond_wait(&t.cond, &t.lock);
    pthread_mutex_unlock(&t.lock);
    /* what the queue has taken is still on its way through the server and the driver */
    usleep(400000);
    AudioQueueStop(q, true);
    AudioQueueDispose(q, true);

    double span = t.lastAt - t.firstAt;
    fprintf(stderr, "pdtone[%s]: done: %ld frames in %d buffers, %.2f s from start to the last buffer; "
            "taken at %.0f frames/s over the last %.2f s (asked %.0f)\n", t.name, t.returned, t.callbacks,
            t.lastAt - t.start, t.firstAt > 0 && span > 0 ? (t.returned - t.firstReturned) / span : 0.0,
            t.firstAt > 0 ? span : 0.0, t.rate);
    return 0;
}
