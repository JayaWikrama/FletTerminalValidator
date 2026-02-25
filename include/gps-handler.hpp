#ifndef __GPS_HANDLER_HPP__
#define __GPS_HANDLER_HPP__

#include <functional>
#include <thread>
#include <mutex>
#include "gps/include/gps.hpp"

class ASA;
class Rmc;

class GpsHandler
{
private:
    bool isRun;
    Gps gps;
    ASA &asa;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

public:
    GpsHandler(ASA &asa);
    ~GpsHandler();

    bool isRuning() const;

    void begin();
    void stop();

    void access(std::function<void(const Nmea &nmea)> accessHandler);

    static void rmcUpdateCallback(const Rmc &rmc, void *userData);
};

#endif