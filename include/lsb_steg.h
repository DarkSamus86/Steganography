#ifndef LSB_STEG_H
#define LSB_STEG_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        LSB_OK = 0,
        LSB_ERR_OPEN = 1,
        LSB_ERR_FORMAT = 2,
        LSB_ERR_CAPACITY = 3,
        LSB_ERR_IO = 4
    } LsbStatus;

    /*
     * Encode: скрыть текстовый файл inside_txt_path внутрь BMP:
     *  in_bmp_path -> out_bmp_path
     * ВАЖНО: работает корректно с 24-bit BMP (без сжатия).
     *
     * Формат данных: сначала 4 байта длины (uint32 little-endian),
     * затем байты сообщения.
     */
    LsbStatus lsb_encode_bmp24(
        const char* in_bmp_path,
        const char* out_bmp_path,
        const char* inside_txt_path
    );

    /*
     * Decode: извлечь текст из BMP и сохранить в out_txt_path.
     */
    LsbStatus lsb_decode_bmp24(
        const char* in_bmp_path,
        const char* out_txt_path
    );

    /* Удобная функция: человекочитаемая строка статуса */
    const char* lsb_status_str(LsbStatus st);

#ifdef __cplusplus
}
#endif

#endif // LSB_STEG_H
