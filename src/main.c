#include <stdio.h>
#include <string.h>
#include "../include/lsb_steg.h"

static void usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s -e <in.bmp> <out.bmp> <message.txt>\n", prog);
    printf("  %s -d <in.bmp> <out.txt>\n", prog);
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "-e") == 0) {
        if (argc != 5) { usage(argv[0]); return 1; }
        LsbStatus st = lsb_encode_bmp24(argv[2], argv[3], argv[4]);
        if (st != LSB_OK) {
            fprintf(stderr, "Encode error: %s\n", lsb_status_str(st));
            return 1;
        }
        printf("OK: embedded message into %s\n", argv[3]);
        return 0;
    }

    if (strcmp(argv[1], "-d") == 0) {
        if (argc != 4) { usage(argv[0]); return 1; }
        LsbStatus st = lsb_decode_bmp24(argv[2], argv[3]);
        if (st != LSB_OK) {
            fprintf(stderr, "Decode error: %s\n", lsb_status_str(st));
            return 1;
        }
        printf("OK: extracted message into %s\n", argv[3]);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
