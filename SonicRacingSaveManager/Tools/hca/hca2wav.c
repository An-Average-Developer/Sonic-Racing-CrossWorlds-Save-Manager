// Decodes a CRI HCA stream to a 16-bit PCM WAV with vgmstream's clHCA (clhca.c, ISC license: see COPYING).
// ExtractGameImages.py builds and runs it for the festival horns.
//   hca2wav <in.hca> <out.wav> <keycode hex> <awb subkey hex>
#include <stdio.h>
#include <stdlib.h>
#include "clhca.h"

static void put32(FILE* f, unsigned v) { fwrite(&v, 4, 1, f); }
static void put16(FILE* f, unsigned short v) { fwrite(&v, 2, 1, f); }

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "hca2wav <in.hca> <out.wav> <keycode hex> <awb subkey hex>\n");
        return 1;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* d = malloc(n);
    if (!d || fread(d, 1, n, f) != (size_t)n) return 1;
    fclose(f);

    // The AWB's subkey derives the stream's key from the game's keycode
    unsigned long long key = strtoull(argv[3], 0, 16);
    unsigned sub = (unsigned)strtoul(argv[4], 0, 16);
    if (sub) key *= ((unsigned long long)sub << 16) | (unsigned short)(~sub + 2);

    clHCA* hca = clHCA_new();
    clHCA_stInfo info;
    if (clHCA_DecodeHeader(hca, d, (unsigned)n) < 0 || clHCA_getInfo(hca, &info) < 0) {
        fprintf(stderr, "not an HCA stream\n");
        return 2;
    }
    clHCA_SetKey(hca, key);

    unsigned ch = info.channelCount, per = info.samplesPerBlock;
    short* pcm = malloc((size_t)info.blockCount * per * ch * sizeof(short));
    size_t total = 0;
    for (unsigned b = 0; b < info.blockCount; b++) {
        size_t at = info.headerSize + (size_t)b * info.blockSize;
        if (at + info.blockSize > (size_t)n) break;
        if (clHCA_DecodeBlock(hca, d + at, info.blockSize) < 0) {
            fprintf(stderr, "block %u: bad key or data\n", b);
            return 3;
        }
        clHCA_ReadSamples16(hca, pcm + total * ch);
        total += per;
    }
    // Drop the encoder's delay and padding
    size_t skip = info.encoderDelay < total ? info.encoderDelay : total;
    size_t keep = total - skip > info.encoderPadding ? total - skip - info.encoderPadding : total - skip;

    FILE* o = fopen(argv[2], "wb");
    if (!o) return 1;
    unsigned bytes = (unsigned)(keep * ch * sizeof(short));
    fwrite("RIFF", 1, 4, o); put32(o, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, o); put32(o, 16); put16(o, 1); put16(o, (unsigned short)ch);
    put32(o, info.samplingRate); put32(o, info.samplingRate * ch * 2); put16(o, (unsigned short)(ch * 2)); put16(o, 16);
    fwrite("data", 1, 4, o); put32(o, bytes);
    fwrite(pcm + skip * ch, 1, bytes, o);
    fclose(o);
    clHCA_delete(hca);
    return 0;
}
