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
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return false;

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0)
    {
        close(sockfd);
        return false;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(sockfd);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(53);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0)
    {
        close(sockfd);
        return false;
    }

    int result = connect(sockfd,
                         reinterpret_cast<sockaddr *>(&serverAddr),
                         sizeof(serverAddr));

    if (result < 0 && errno != EINPROGRESS)
    {
        close(sockfd);
        return false;
    }

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sockfd, &writeSet);

    timeval timeout{};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    result = select(sockfd + 1, nullptr, &writeSet, nullptr, &timeout);

    if (result <= 0)
    {
        close(sockfd);
        return false;
    }

    int soError = 0;
    socklen_t len = sizeof(soError);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &soError, &len);

    close(sockfd);

    return (soError == 0);
}

GsmHandler::GsmHandler() : isRun(false),
                           connected(false),
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
            bool isConnectedTmp = false;
            std::time_t lastConnectionTest = 0;
            std::time_t currentTime = std::time(nullptr) - (2 * INTERNET_CONNECTION_CHECK_INTERVAL);
            /* check is connected */
            isConnectedTmp = testInternetConnection();
            {
                std::lock_guard<std::mutex> guard(this->mtx);
                this->connected = isConnectedTmp;
            }
            if (this->connected)
            {
                std::lock_guard<std::mutex> guard(this->mtx);
                this->signalStrength = gprs_sigval_get();
                if (this->signalStrength == 99)
                    this->signalStrength = 24;
            }
            if (this->connected)
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
                    isConnectedTmp = testInternetConnection();
                    std::lock_guard<std::mutex> guard(this->mtx);
                    this->connected = isConnectedTmp;
                    if (this->connected)
                        lastConnectionTest = currentTime;
                }
                if (this->connected)
                {
                    {
                        std::lock_guard<std::mutex> guard(this->mtx);
                        this->signalStrength = gprs_sigval_get();
                        if (this->signalStrength == 99)
                            this->signalStrength = 24;
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

bool GsmHandler::isConnected() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->connected;
}

int GsmHandler::getSignalStrength() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->signalStrength;
}