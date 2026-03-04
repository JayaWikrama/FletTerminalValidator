#include <png.h>
#include <cstdio>
#include <cstring>

extern "C"
{
#include "quirc/include/quirc.h"
}

#include "qr-decode-helper.hpp"

QRDecodeHelper::QRDecodeHelper()
    : width(0),
      height(0)
{
}

QRDecodeHelper::~QRDecodeHelper()
{
}

void QRDecodeHelper::clear()
{
    imageBuffer.clear();
    width = 0;
    height = 0;
}

bool QRDecodeHelper::loadPngToGrayscale(const std::string &filePath)
{
    clear();

    FILE *fp = fopen(filePath.c_str(), "rb");
    if (!fp)
        return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png)
    {
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        png_destroy_read_struct(&png, nullptr, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);

    // Pastikan format grayscale 8-bit
    png_set_strip_alpha(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB ||
        png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB_ALPHA)
        png_set_rgb_to_gray_fixed(png, 1, -1, -1);

    if (png_get_bit_depth(png, info) == 16)
        png_set_strip_16(png);

    png_read_update_info(png, info);

    imageBuffer.resize(width * height);

    std::vector<png_bytep> rows(height);
    for (int y = 0; y < height; ++y)
        rows[y] = imageBuffer.data() + y * width;

    png_read_image(png, rows.data());

    png_destroy_read_struct(&png, &info, nullptr);
    fclose(fp);

    return true;
}

bool QRDecodeHelper::decodeQrBuffer(std::string &outputText)
{
    if (imageBuffer.empty() || width <= 0 || height <= 0)
        return false;

    struct quirc *qr = quirc_new();
    if (!qr)
        return false;

    if (quirc_resize(qr, width, height) < 0)
    {
        quirc_destroy(qr);
        return false;
    }

    uint8_t *buffer = quirc_begin(qr, nullptr, nullptr);
    std::memcpy(buffer, imageBuffer.data(), width * height);
    quirc_end(qr);

    int count = quirc_count(qr);
    if (count <= 0)
    {
        quirc_destroy(qr);
        return false;
    }

    struct quirc_code code;
    struct quirc_data data;

    quirc_extract(qr, 0, &code);

    if (quirc_decode(&code, &data) != 0)
    {
        quirc_destroy(qr);
        return false;
    }

    outputText.assign(reinterpret_cast<char *>(data.payload),
                      data.payload_len);

    quirc_destroy(qr);
    return true;
}

bool QRDecodeHelper::parseFromPng(const std::string &filePath,
                                  std::string &outputText)
{
    if (!loadPngToGrayscale(filePath))
        return false;

    return decodeQrBuffer(outputText);
}

bool QRDecodeHelper::parseFromBuffer(const uint8_t *buffer,
                                     int bufferWidth,
                                     int bufferHeight,
                                     std::string &outputText)
{
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0)
        return false;

    width = bufferWidth;
    height = bufferHeight;

    imageBuffer.assign(buffer, buffer + (width * height));

    return decodeQrBuffer(outputText);
}