#include "gsm-handler.hpp"
#include "utils/include/debug.hpp"
#include "reader/include/LinuxHardwareDriver.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define INTERNET_CONNECTION_CHECK_INTERVAL 120

static bool testInternetConnection()
{
    const char *ip = "8.8.8.8";
    int sockfd = -1;
    struct sockaddr_in serverAddr{};

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return false;

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(53);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0)
    {
        close(sockfd);
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}

GsmHandler::GsmHandler() : isRun(false),
                           signalStrength(0),
                           th(),
                           mtx() {}

GsmHandler::~GsmHandler()
{
    this->stop();
}

bool GsmHandler::isRuning() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->isRun;
}

void GsmHandler::begin()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = true;
    }
    this->th.reset(new std::thread(
        [this]()
        {
            bool isConnected = true;
            std::time_t lastConnectionTest = 0;
            std::time_t currentTime = std::time(nullptr) - (2 * INTERNET_CONNECTION_CHECK_INTERVAL);
            /* check is connected */
            isConnected = testInternetConnection();
            if (isConnected)
            {
                std::lock_guard<std::mutex> guard(this->mtx);
                this->signalStrength = gprs_sigval_get();
            }
            if (isConnected && this->signalStrength > 0 && this->signalStrength != 99)
            {
                Debug::info(__FILE__, __LINE__, __func__, "gsm (gprs) already connected with signal strength %d\n", this->signalStrength);
            }
            else
            {
                /* connect */
                while (this->isRuning() && gprs_net_connect())
                {
                    Debug::warning(__FILE__, __LINE__, __func__, "gsm (gprs) connection failed\n");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    Debug::info(__FILE__, __LINE__, __func__, "try to connect gsm (gprs)...\n");
                }
            }
            /* network monitoring */
            while (this->isRuning())
            {
                currentTime = std::time(nullptr);
                if (std::abs(static_cast<int>(difftime(currentTime, lastConnectionTest))) > INTERNET_CONNECTION_CHECK_INTERVAL)
                {
                    isConnected = testInternetConnection();
                    if (isConnected)
                        lastConnectionTest = currentTime;
                }
                if (isConnected)
                {
                    {
                        std::lock_guard<std::mutex> guard(this->mtx);
                        this->signalStrength = gprs_sigval_get();
                        Debug::info(__FILE__, __LINE__, __func__, "gsm (gprs) signal strength: %d\n", this->signalStrength);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                }
                else
                {
                    {
                        std::lock_guard<std::mutex> guard(this->mtx);
                        this->signalStrength = 0;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                }
            }
        }));
}

void GsmHandler::stop()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = false;
    }
    this->th->join();
    this->th.reset();
}

int GsmHandler::getSignalStrength() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->signalStrength;
}