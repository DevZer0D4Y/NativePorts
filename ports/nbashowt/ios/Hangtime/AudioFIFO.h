#ifndef SHOWTIME_AUDIO_FIFO_H
#define SHOWTIME_AUDIO_FIFO_H
#include <stdint.h>
#include <string.h>

enum { HT_AUDIO_CAPACITY = 8192, HT_AUDIO_PREROLL = 4096, HT_AUDIO_FADE = 128 };
typedef struct {
    int16_t samples[HT_AUDIO_CAPACITY * 2];
    unsigned head, count, underruns, dropped;
    unsigned fade, splice;
    float last_left, last_right, splice_left, splice_right;
    int ready;
} HTAudioFIFO;

/* Caller holds the audio mutex. Keep the newest samples on overflow rather
   than throwing away the entire queue every time it exceeds 100 ms. */
static inline void ht_fifo_push(HTAudioFIFO *q, const int16_t *samples, size_t frames) {
    int overflow = frames > HT_AUDIO_CAPACITY || frames + q->count > HT_AUDIO_CAPACITY;
    if (overflow && q->ready) {
        q->splice = HT_AUDIO_FADE;
        q->splice_left = q->last_left; q->splice_right = q->last_right;
    }
    if (frames > HT_AUDIO_CAPACITY) {
        size_t skip = frames - HT_AUDIO_CAPACITY;
        samples += skip * 2; frames -= skip; q->dropped += (unsigned)skip;
    }
    unsigned drop = q->count + (unsigned)frames > HT_AUDIO_CAPACITY
                  ? q->count + (unsigned)frames - HT_AUDIO_CAPACITY : 0;
    q->head = (q->head + drop) % HT_AUDIO_CAPACITY;
    q->count -= drop; q->dropped += drop;
    unsigned tail = (q->head + q->count) % HT_AUDIO_CAPACITY;
    size_t first = frames < HT_AUDIO_CAPACITY - tail ? frames : HT_AUDIO_CAPACITY - tail;
    memcpy(q->samples + tail * 2, samples, first * 2 * sizeof(int16_t));
    memcpy(q->samples, samples + first * 2, (frames - first) * 2 * sizeof(int16_t));
    q->count += (unsigned)frames;
}

/* A small preroll absorbs frame-sized production jitter. When emulation
   actually falls behind, fade out and re-prime instead of clicking between
   full-volume samples and zero. This does not change game or audio clocks. */
static inline void ht_fifo_read(HTAudioFIFO *q, float *left, float *right, unsigned frames) {
    if (!q->ready && q->count >= HT_AUDIO_PREROLL) { q->ready = 1; q->fade = 0; }
    for (unsigned i = 0; i < frames; ++i) {
        left[i] = right[i] = 0;
        if (!q->ready) { q->last_left = q->last_right = 0; q->splice = 0; continue; }
        if (!q->count) { q->ready = 0; q->fade = 0; q->underruns++; q->splice = 0; q->last_left = q->last_right = 0; continue; }
        if (q->fade < HT_AUDIO_FADE) q->fade++;
        unsigned gain = q->fade < q->count ? q->fade : q->count;
        if (gain > HT_AUDIO_FADE) gain = HT_AUDIO_FADE;
        float scale = gain / (32768.0f * HT_AUDIO_FADE);
        left[i] = q->samples[q->head * 2] * scale;
        right[i] = q->samples[q->head * 2 + 1] * scale;
        if (q->splice) {
            float mix = (HT_AUDIO_FADE - q->splice + 1) / (float)HT_AUDIO_FADE;
            left[i] = q->splice_left + (left[i] - q->splice_left) * mix;
            right[i] = q->splice_right + (right[i] - q->splice_right) * mix;
            q->splice--;
        }
        q->last_left = left[i]; q->last_right = right[i];
        q->head = (q->head + 1) % HT_AUDIO_CAPACITY; q->count--;
    }
}
#endif
