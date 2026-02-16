#ifndef __GSM_HANDLER_HPP__
#define __GSM_HANDLER_HPP__

#include <functional>
#include <thread>
#include <mutex>

class GsmHandler
{
private:
    bool isRun;
    int signalStrength;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

public:
    GsmHandler();
    ~GsmHandler();

    bool isRuning() const;

    void begin();
    void stop();

    int getSignalStrength() const;
};

#endif