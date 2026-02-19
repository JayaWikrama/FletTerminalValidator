#ifndef __GPS_HANDLER_HPP__
#define __GPS_HANDLER_HPP__

#include <functional>
#include <thread>
#include <mutex>
#include "gps/include/gps.hpp"

class GpsHandler
{
private:
    bool isRun;
    Gps gps;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

public:
    GpsHandler();
    ~GpsHandler();

    bool isRuning() const;

    void begin();
    void stop();

    void access(std::function<void(const Nmea &nmea)> accessHandler);
};

#endif