#include "../include/lsb_steg.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Безопасное чтение little-endian чисел из BMP-заголовка */
static int read_u16_le(FILE* f, long off, uint16_t* out) {
    if (fseek(f, off, SEEK_SET) != 0) return 0;
    int b0 = fgetc(f), b1 = fgetc(f);
    if (b0 == EOF || b1 == EOF) return 0;
    *out = (uint16_t)(b0 | (b1 << 8));
    return 1;
}

static int read_u32_le(FILE* f, long off, uint32_t* out) {
    if (fseek(f, off, SEEK_SET) != 0) return 0;
    int b0 = fgetc(f), b1 = fgetc(f), b2 = fgetc(f), b3 = fgetc(f);
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) return 0;
    *out = (uint32_t)( (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24) );
    return 1;
}

static int write_u32_le(FILE* f, uint32_t v) {
    if (fputc((int)(v & 0xFF), f) == EOF) return 0;
    if (fputc((int)((v >> 8) & 0xFF), f) == EOF) return 0;
    if (fputc((int)((v >> 16) & 0xFF), f) == EOF) return 0;
    if (fputc((int)((v >> 24) & 0xFF), f) == EOF) return 0;
    return 1;
}

static long file_size(FILE* f) {
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, cur, SEEK_SET);
    return sz;
}

/*
BMP базовые оффсеты:
- 0..1: 'B''M'
- 10..13: bfOffBits (offset до пикселей)
DIB header:
- 14..17: DIB header size (обычно 40)
- 28..29: bitsPerPixel
- 30..33: compression (0 = BI_RGB)
*/
typedef struct {
    uint32_t offBits;
    uint16_t bpp;
    uint32_t compression;
    long bmpSize;
} BmpInfo;

static int read_bmp_info(FILE* bmp, BmpInfo* info) {
    if (!info) return 0;

    if (fseek(bmp, 0, SEEK_SET) != 0) return 0;
    int b0 = fgetc(bmp), b1 = fgetc(bmp);
    if (b0 != 'B' || b1 != 'M') return 0;

    if (!read_u32_le(bmp, 10, &info->offBits)) return 0;
    if (!read_u16_le(bmp, 28, &info->bpp)) return 0;
    if (!read_u32_le(bmp, 30, &info->compression)) return 0;

    info->bmpSize = file_size(bmp);

    /* поддерживаем только 24-bit без сжатия */
    if (info->bpp != 24) return 0;
    if (info->compression != 0) return 0;
    if ((long)info->offBits <= 0 || (long)info->offBits >= info->bmpSize) return 0;

    return 1;
}

/* Вместимость: каждый байт пиксельных данных несёт 1 бит. */
static int has_capacity(const BmpInfo* bi, uint32_t msg_len) {
    long pixel_bytes = bi->bmpSize - (long)bi->offBits;
    /* 4 байта длины + msg_len байт данных => (4+msg_len)*8 бит */
    uint64_t need_bits = (uint64_t)(4 + msg_len) * 8ULL;
    return (uint64_t)pixel_bytes >= need_bits;
}

static int embed_bit(FILE* in, FILE* out, int bit) {
    int c = fgetc(in);
    if (c == EOF) return 0;
    unsigned char b = (unsigned char)c;
    b = (unsigned char)((b & 0xFE) | (bit & 1));
    if (fputc((int)b, out) == EOF) return 0;
    return 1;
}

static int extract_bit(FILE* in, int* bit_out) {
    int c = fgetc(in);
    if (c == EOF) return 0;
    *bit_out = (c & 1);
    return 1;
}

static int embed_byte(FILE* in, FILE* out, unsigned char byte) {
    /* от старшего бита к младшему */
    for (int i = 7; i >= 0; --i) {
        int bit = (byte >> i) & 1;
        if (!embed_bit(in, out, bit)) return 0;
    }
    return 1;
}

static int extract_byte(FILE* in, unsigned char* out_byte) {
    unsigned char v = 0;
    for (int i = 0; i < 8; ++i) {
        int bit;
        if (!extract_bit(in, &bit)) return 0;
        v = (unsigned char)((v << 1) | (bit & 1));
    }
    *out_byte = v;
    return 1;
}

const char* lsb_status_str(LsbStatus st) {
    switch (st) {
        case LSB_OK: return "OK";
        case LSB_ERR_OPEN: return "File open/create error";
        case LSB_ERR_FORMAT: return "Unsupported BMP format (need 24-bit, uncompressed)";
        case LSB_ERR_CAPACITY: return "Not enough capacity in image to hide this message";
        case LSB_ERR_IO: return "I/O error while processing";
        default: return "Unknown error";
    }
}

LsbStatus lsb_encode_bmp24(const char* in_bmp_path, const char* out_bmp_path, const char* inside_txt_path) {
    FILE* in = fopen(in_bmp_path, "rb");
    if (!in) return LSB_ERR_OPEN;

    FILE* msg = fopen(inside_txt_path, "rb");
    if (!msg) { fclose(in); return LSB_ERR_OPEN; }

    FILE* out = fopen(out_bmp_path, "wb");
    if (!out) { fclose(in); fclose(msg); return LSB_ERR_OPEN; }

    BmpInfo bi;
    if (!read_bmp_info(in, &bi)) {
        fclose(in); fclose(msg); fclose(out);
        return LSB_ERR_FORMAT;
    }

    long msg_sz_l = file_size(msg);
    if (msg_sz_l < 0 || msg_sz_l > 0xFFFFFFFFL) {
        fclose(in); fclose(msg); fclose(out);
        return LSB_ERR_IO;
    }
    uint32_t msg_len = (uint32_t)msg_sz_l;

    if (!has_capacity(&bi, msg_len)) {
        fclose(in); fclose(msg); fclose(out);
        return LSB_ERR_CAPACITY;
    }

    /* 1) копируем заголовок как есть до offBits */
    if (fseek(in, 0, SEEK_SET) != 0) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    for (uint32_t i = 0; i < bi.offBits; ++i) {
        int c = fgetc(in);
        if (c == EOF) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
        if (fputc(c, out) == EOF) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    }

    /* 2) в пиксельные данные встраиваем: [len:4 bytes LE] + [message bytes] */
    unsigned char len_bytes[4];
    len_bytes[0] = (unsigned char)(msg_len & 0xFF);
    len_bytes[1] = (unsigned char)((msg_len >> 8) & 0xFF);
    len_bytes[2] = (unsigned char)((msg_len >> 16) & 0xFF);
    len_bytes[3] = (unsigned char)((msg_len >> 24) & 0xFF);

    for (int i = 0; i < 4; ++i) {
        if (!embed_byte(in, out, len_bytes[i])) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    }

    /* message bytes */
    if (fseek(msg, 0, SEEK_SET) != 0) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    for (uint32_t k = 0; k < msg_len; ++k) {
        int c = fgetc(msg);
        if (c == EOF) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
        if (!embed_byte(in, out, (unsigned char)c)) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    }

    /* 3) остаток пиксельных данных копируем как есть */
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (fputc(c, out) == EOF) { fclose(in); fclose(msg); fclose(out); return LSB_ERR_IO; }
    }

    fclose(in);
    fclose(msg);
    fclose(out);
    return LSB_OK;
}

LsbStatus lsb_decode_bmp24(const char* in_bmp_path, const char* out_txt_path) {
    FILE* in = fopen(in_bmp_path, "rb");
    if (!in) return LSB_ERR_OPEN;

    FILE* out = fopen(out_txt_path, "wb");
    if (!out) { fclose(in); return LSB_ERR_OPEN; }

    BmpInfo bi;
    if (!read_bmp_info(in, &bi)) {
        fclose(in); fclose(out);
        return LSB_ERR_FORMAT;
    }

    /* перейти к началу пиксельных данных */
    if (fseek(in, (long)bi.offBits, SEEK_SET) != 0) { fclose(in); fclose(out); return LSB_ERR_IO; }

    /* 1) читаем длину (4 байта LE), каждый байт собран из 8 LSB */
    unsigned char len_bytes[4];
    for (int i = 0; i < 4; ++i) {
        if (!extract_byte(in, &len_bytes[i])) { fclose(in); fclose(out); return LSB_ERR_IO; }
    }
    uint32_t msg_len = (uint32_t)(
        (uint32_t)len_bytes[0] |
        ((uint32_t)len_bytes[1] << 8) |
        ((uint32_t)len_bytes[2] << 16) |
        ((uint32_t)len_bytes[3] << 24)
    );

    if (!has_capacity(&bi, msg_len)) {
        /* длина может быть мусором, если в картинку ничего не вшивали */
        fclose(in); fclose(out);
        return LSB_ERR_CAPACITY;
    }

    /* 2) извлекаем msg_len байт */
    for (uint32_t k = 0; k < msg_len; ++k) {
        unsigned char ch;
        if (!extract_byte(in, &ch)) { fclose(in); fclose(out); return LSB_ERR_IO; }
        if (fputc((int)ch, out) == EOF) { fclose(in); fclose(out); return LSB_ERR_IO; }
    }

    fclose(in);
    fclose(out);
    return LSB_OK;
}
