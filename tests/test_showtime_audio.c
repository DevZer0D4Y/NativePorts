#include "../ports/nbashowt/ios/Hangtime/AudioFIFO.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    HTAudioFIFO *q = calloc(1, sizeof *q);
    int16_t samples[4096 * 2];
    float l[4096], r[4096];
    for (unsigned i = 0; i < 4096; ++i) { samples[i*2] = 16384; samples[i*2+1] = -8192; }
    ht_fifo_push(q, samples, 800);
    ht_fifo_read(q, l, r, 512);
    assert(q->count == 800 && l[0] == 0); // no premature start
    ht_fifo_push(q, samples, 3800);
    ht_fifo_read(q, l, r, 512);
    assert(q->count == 4088 && l[127] == 0.5f && r[127] == -0.25f);
    ht_fifo_read(q, l, r, 4096); // force a real starvation
    assert(q->underruns == 1 && !q->ready && q->count == 0);
    assert(l[4087] > 0 && l[4087] < 0.004f && l[4088] == 0);
    // Normal frame-sized production remains continuous after priming.
    ht_fifo_push(q, samples, 4096);
    for (unsigned i = 0; i < 200; ++i) {
        ht_fifo_push(q, samples, 800);
        ht_fifo_read(q, l, r, 800);
        assert(l[799] == 0.5f && r[799] == -0.25f && q->underruns == 1);
    }
    // Overflow drops only the oldest excess; it never empties the queue.
    memset(q, 0, sizeof *q);
    for (unsigned i = 0; i < 10; ++i) ht_fifo_push(q, samples, 4096);
    assert(q->count == HT_AUDIO_CAPACITY && q->dropped == 40960 - HT_AUDIO_CAPACITY);
    for (unsigned i = 0; i < 9; ++i) ht_fifo_read(q, l, r, 4096);
    assert(q->count == 0 && q->underruns == 1);
    // Overflow transitions from the last emitted sample, independently per channel.
    memset(q, 0, sizeof *q);
    ht_fifo_push(q, samples, 4096);
    ht_fifo_read(q, l, r, 512);
    for (unsigned i = 0; i < 4096; ++i) { samples[i*2] = -16384; samples[i*2+1] = 8192; }
    for (unsigned i = 0; i < 4; ++i) ht_fifo_push(q, samples, 4096);
    assert(q->splice == 128 && q->count == 8192);
    ht_fifo_read(q, l, r, 128);
    assert(fabsf(l[0] - (0.5f - 1.0f / 128)) < 0.00001f);
    assert(fabsf(r[0] - (-0.25f + 0.5f / 128)) < 0.00001f);
    assert(l[127] == -0.5f && r[127] == 0.25f && q->splice == 0);
    ht_fifo_read(q, l, r, 512);
    for (unsigned i = 0; i < 512; ++i) assert(l[i] == -0.5f && r[i] == 0.25f);
    free(q);
    puts("PASS: audio preroll, steady playback, starvation fade, wraparound and bounded overflow");
}
