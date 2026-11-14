// SPDX-License-Identifier: MIT
// sf-prof: a sampling profiler of a program's main thread, for systems without sample(1) or dtrace.
//
//   DYLD_INSERT_LIBRARIES=/path/libsfprof.dylib program
//   touch /tmp/sfprof.on        sampling starts (every SFPROF_US microseconds, default 1000)
//   rm /tmp/sfprof.on           sampling stops; the report is written to /tmp/sfprof.txt
//                               (SFPROF_OUT=path): functions by samples where they were running
//                               (self) and by samples where they were on the stack (total)
//   SFPROF_FOCUS=name           the report counts only the samples with NAME on their stack
//
// The main thread is suspended for each sample (thread_suspend, thread_get_state, a walk of the
// frame-pointer chain, thread_resume). Names are what dladdr finds: the nearest symbol of the
// image, so a function without a symbol shows as the one before it, with a large offset.
#include <dlfcn.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEPTH 32
#define MAXSAMPLES 60000

static thread_act_t mainThread;
static uintptr_t (*samples)[DEPTH];
static unsigned char *depths;
static unsigned count;

typedef struct { const char *name; unsigned self, total, last; } Entry;

static const char *nameOf(uintptr_t pc, char *buf, size_t size) {
    Dl_info info;
    if (pc != 0 && dladdr((void *)pc, &info) && info.dli_sname != NULL) {
        snprintf(buf, size, "%s", info.dli_sname);
        return buf;
    }
    if (pc != 0 && dladdr((void *)pc, &info) && info.dli_fname != NULL) {
        const char *leaf = strrchr(info.dli_fname, '/');
        snprintf(buf, size, "%s+0x%lx", leaf ? leaf + 1 : info.dli_fname, (unsigned long)(pc - (uintptr_t)info.dli_fbase));
        return buf;
    }
    snprintf(buf, size, "0x%lx", (unsigned long)pc);
    return buf;
}

static int byTotal(const void *a, const void *b) { return (int)((const Entry *)b)->total - (int)((const Entry *)a)->total; }
static int bySelf(const void *a, const void *b) { return (int)((const Entry *)b)->self - (int)((const Entry *)a)->self; }

static void report(void) {
    const char *path = getenv("SFPROF_OUT") ? getenv("SFPROF_OUT") : "/tmp/sfprof.txt";
    FILE *out = fopen(path, "w");
    if (out == NULL) return;
    unsigned cap = 8192, used = 0;
    Entry *entries = calloc(cap, sizeof(Entry));
    char buf[512];
    const char *focus = getenv("SFPROF_FOCUS");
    unsigned kept = 0;
    for (unsigned s = 0; s < count; s++) {
        if (focus != NULL) {
            unsigned d;
            for (d = 0; d < depths[s]; d++) {
                char *plus;
                nameOf(samples[s][d] - (d > 0 ? 4 : 0), buf, sizeof(buf));
                if ((plus = strchr(buf, '+')) != NULL) *plus = 0;
                if (strcmp(buf, focus) == 0) break;
            }
            if (d == depths[s]) continue;
        }
        kept++;
        for (unsigned d = 0; d < depths[s]; d++) {
            // a return address points after the call: look the caller up one instruction back
            uintptr_t pc = samples[s][d] - (d > 0 ? 4 : 0);
            const char *name = nameOf(pc, buf, sizeof(buf));
            // names with an offset are kept per image, not per address
            char *plus = strchr(buf, '+');
            if (plus && strncmp(buf, "0x", 2) != 0) *plus = 0;
            unsigned i;
            for (i = 0; i < used && strcmp(entries[i].name, name) != 0; i++) {}
            if (i == used) {
                if (used == cap) continue;
                entries[used].name = strdup(name);
                used++;
            }
            if (d == 0) entries[i].self++;
            if (entries[i].last != s + 1) { entries[i].total++; entries[i].last = s + 1; }
        }
    }
    fprintf(out, "SFPROF samples=%u kept=%u focus=%s interval_us=%s\n", count, kept, focus ? focus : "-", getenv("SFPROF_US") ? getenv("SFPROF_US") : "1000");
    qsort(entries, used, sizeof(Entry), bySelf);
    fprintf(out, "-- self\n");
    for (unsigned i = 0; i < used && i < 60; i++)
        if (entries[i].self) fprintf(out, "%6u %5.1f%%  %s\n", entries[i].self, 100.0 * entries[i].self / (count ? count : 1), entries[i].name);
    qsort(entries, used, sizeof(Entry), byTotal);
    fprintf(out, "-- total\n");
    for (unsigned i = 0; i < used && i < 120; i++)
        fprintf(out, "%6u %5.1f%%  %s\n", entries[i].total, 100.0 * entries[i].total / (count ? count : 1), entries[i].name);
    fclose(out);
    for (unsigned i = 0; i < used; i++) free((void *)entries[i].name);
    free(entries);
}

static void *sampler(void *unused) {
    useconds_t interval = getenv("SFPROF_US") ? (useconds_t)atoi(getenv("SFPROF_US")) : 1000;
    int on = 0;
    for (;;) {
        int now = access("/tmp/sfprof.on", F_OK) == 0;
        if (on && !now) { report(); count = 0; }
        on = now;
        if (!on) { usleep(20000); continue; }
        if (count < MAXSAMPLES && thread_suspend(mainThread) == KERN_SUCCESS) {
            arm_thread_state64_t state;
            mach_msg_type_number_t n = ARM_THREAD_STATE64_COUNT;
            if (thread_get_state(mainThread, ARM_THREAD_STATE64, (thread_state_t)&state, &n) == KERN_SUCCESS) {
                unsigned d = 0;
                samples[count][d++] = (uintptr_t)__darwin_arm_thread_state64_get_pc(state);
                samples[count][d++] = (uintptr_t)__darwin_arm_thread_state64_get_lr(state);
                uintptr_t fp = (uintptr_t)__darwin_arm_thread_state64_get_fp(state);
                while (d < DEPTH && fp != 0 && (fp & 7) == 0) {
                    uintptr_t *frame = (uintptr_t *)fp;
                    uintptr_t next = frame[0], ret = frame[1];
                    if (ret == 0 || next <= fp) break;
                    samples[count][d++] = ret;
                    fp = next;
                }
                depths[count] = (unsigned char)d;
                count++;
            }
            thread_resume(mainThread);
        }
        usleep(interval);
    }
    return NULL;
}

__attribute__((constructor)) static void start(void) {
    mainThread = mach_thread_self();
    samples = calloc(MAXSAMPLES, sizeof(*samples));
    depths = calloc(MAXSAMPLES, 1);
    pthread_t t;
    if (samples && depths) pthread_create(&t, NULL, sampler, NULL);
}
