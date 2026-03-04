#ifndef __QR_DECODE_HELPER_HPP__
#define __QR_DECODE_HELPER_HPP__

#include <string>
#include <vector>
#include <cstdint>

class QRDecodeHelper
{
private:
    std::vector<uint8_t> imageBuffer;
    int width;
    int height;

    bool loadPngToGrayscale(const std::string &filePath);
    bool decodeQrBuffer(std::string &outputText);

public:
    QRDecodeHelper();
    ~QRDecodeHelper();

    bool parseFromPng(const std::string &filePath, std::string &outputText);

    bool parseFromBuffer(const uint8_t *buffer, int bufferWidth, int bufferHeight, std::string &outputText);

    void clear();
};

#endif