#ifndef __SETUP_HPP__
#define __SETUP_HPP__

#include <string>

#include "qrscanner/include/qr-scanner.hpp"

class Setup
{
private:
    std::string imei;
    QRScanner qrsc;

    bool validateQRFormat(const std::string &qrRawData, std::string &urlOut, std::string &jsonOut);
    std::string extractBaseUrl(const std::string &fullUrl);
    bool buildConfigFile(const std::string &url, const std::string &qrJsonString, const std::string &outputPath);

public:
    Setup();
    ~Setup();

    void setIMEI(const std::string &emei);
    bool loadIMEI(const std::string &file);

    bool setup(const std::string &communicationConfigPath);
};

#endif