#ifndef __GSM_HANDLER_HPP__
#define __GSM_HANDLER_HPP__

#include <functional>
#include <thread>
#include <mutex>
#include <string>

class GsmHandler
{
private:
    bool isRun;
    bool connected;
    int signalStrength;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

    static std::string ntpServer;

public:
    GsmHandler();
    ~GsmHandler();

    bool isRuning() const;

    void begin();
    void stop();

    bool isConnected() const;
    int getSignalStrength() const;

    static void setNTPServer(const std::string &url);
};

#endif