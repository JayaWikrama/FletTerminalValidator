#include <sstream>
#include <fstream>
#include "qr-decode-helper.hpp"
#include "sound-helper.hpp"
#include "setup.hpp"
#include "qrscanner/include/qr-scanner.hpp"
#include "utils/include/debug.hpp"
#include "utils/include/nlohmann/json.hpp"

bool Setup::validateQRFormat(const std::string &qrRawData, std::string &urlOut, std::string &jsonOut)
{
    urlOut.clear();
    jsonOut.clear();

    if (qrRawData.empty())
        return false;

    if (qrRawData.rfind("AFC#", 0) != 0)
    {
        Debug::error(__FILE__, __LINE__, __func__, "invalid header\n");
        return false;
    }

    const size_t expectedHashCount = 6;
    size_t hashCount = 0;

    for (char c : qrRawData)
    {
        if (c == '#')
            ++hashCount;
    }

    if (hashCount != expectedHashCount)
    {
        Debug::error(__FILE__, __LINE__, __func__, "invalid # count (%lu != %lu)\n", hashCount, expectedHashCount);
        return false;
    }

    std::vector<std::string> segments;
    std::stringstream ss(qrRawData);
    std::string item;

    while (std::getline(ss, item, '#'))
    {
        segments.push_back(item);
    }

    if (segments.size() != 7)
    {
        Debug::error(__FILE__, __LINE__, __func__, "segments size is not 7, but %lu\n", segments.size());
        return false;
    }

    if (segments.back().size() > 2)
    {
        Debug::error(__FILE__, __LINE__, __func__, "last segment size > 2\n");
        return false;
    }

    const std::string &url = segments[4];
    const std::string &json = segments[5];

    if (!(url.rfind("https://", 0) == 0 ||
          url.rfind("http://", 0) == 0))
    {
        Debug::error(__FILE__, __LINE__, __func__, "invalid url header\n");
        return false;
    }

    if (url.find('.', 8) == std::string::npos)
    {
        Debug::error(__FILE__, __LINE__, __func__, "domain validation error\n");
        return false;
    }

    if (json.size() < 2)
    {
        Debug::error(__FILE__, __LINE__, __func__, "invalid json size\n");
        return false;
    }

    if (!(json.front() == '{' && json.back() == '}'))
    {
        Debug::error(__FILE__, __LINE__, __func__, "invalid json se\n");
        return false;
    }

    urlOut = url;
    jsonOut = json;

    return true;
}

std::string Setup::extractBaseUrl(const std::string &fullUrl)
{
    size_t schemeEnd = fullUrl.find("://");
    if (schemeEnd == std::string::npos)
        return "";

    schemeEnd += 3;

    size_t pathStart = fullUrl.find('/', schemeEnd);

    if (pathStart == std::string::npos)
        return fullUrl;

    return fullUrl.substr(0, pathStart);
}

bool Setup::buildConfigFile(const std::string &url, const std::string &qrJsonString, const std::string &outputPath)
{
    try
    {
        nlohmann::json qrJson = nlohmann::json::parse(qrJsonString);

        if (!qrJson.contains("device_code") ||
            !qrJson.contains("device_version") ||
            !qrJson.contains("serial_number") ||
            !qrJson.contains("session"))
        {
            Debug::error(__FILE__, __LINE__, __func__, "some json filed is missing\n");
            return false;
        }

        const std::string &serialNumber = qrJson["serial_number"].get<std::string>();
        if (serialNumber.compare(this->imei) != 0)
        {
            Debug::error(__FILE__, __LINE__, __func__, "imei mismatch, %s != %s\n", serialNumber.c_str(), this->imei.c_str());
            return false;
        }

        nlohmann::json outputJson;
        outputJson["base_url"] = this->extractBaseUrl(url) + "/api";
        outputJson["code"] = qrJson["device_code"];
        outputJson["device_version"] = qrJson["device_version"];
        outputJson["serial_number"] = qrJson["serial_number"];
        outputJson["key"] = qrJson["session"];

        std::ofstream file(outputPath);
        if (!file.is_open())
        {
            Debug::error(__FILE__, __LINE__, __func__, "failed to open file\n");
            return false;
        }

        file << outputJson.dump(4);
        file.close();
    }
    catch (const std::exception &e)
    {
        Debug::error(__FILE__, __LINE__, __func__, "%s\n", e.what());
        return false;
    }

    return true;
}

Setup::Setup() : imei(),
                 qrsc("/dev/ttyS4", B115200)
{
    this->qrsc.setup();
}

Setup::~Setup() {}

void Setup::setIMEI(const std::string &emei)
{
    this->imei = imei;
}

bool Setup::loadIMEI(const std::string &file)
{
    QRDecodeHelper qrPng;
    bool result = qrPng.parseFromPng(file, this->imei);
    if (result)
    {
        Debug::info(__FILE__, __LINE__, __func__, "imei: %s\n", this->imei.c_str());
    }
    else
    {
        Debug::error(__FILE__, __LINE__, __func__, "failed to load imei\n");
    }
    return result;
}

bool Setup::setup(const std::string &communicationConfigPath)
{
    bool result = false;

    qrsc.scan()
        .onSuccess(
            [this, &communicationConfigPath, &result](const std::string &payload)
            {
                std::string url;
                std::string jsonStr;
                Debug::info(__FILE__, __LINE__, __func__, "qr payload: %s\n", payload.c_str());
                if (this->validateQRFormat(payload, url, jsonStr))
                {
                    if (this->buildConfigFile(url, jsonStr, communicationConfigPath))
                    {
                        Debug::info(__FILE__, __LINE__, __func__, "success\n");
                        result = true;
                        SoundHelper::beep(1);
                    }
                    else
                    {
                        SoundHelper::beep(2);
                    }
                }
            })
        .onTimeout(
            []()
            {
                Debug::warning(__FILE__, __LINE__, __func__, "timeout\n");
            })
        .onUnInitialized(
            [this]()
            {
                Debug::error(__FILE__, __LINE__, __func__, "uninitialized\n");
                this->qrsc.setup();
            });

    return result;
}