#include "gps-handler.hpp"
#include "utils/include/debug.hpp"

GpsHandler::GpsHandler() : isRun(false),
                           gps("/dev/ttyS1", B9600),
                           th(),
                           mtx() {}

GpsHandler::~GpsHandler()
{
    this->stop();
}

bool GpsHandler::isRuning() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->isRun;
}

void GpsHandler::begin()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = true;
    }
    this->th.reset(new std::thread(
        [this]()
        {
            this->gps.setup();
            while (this->isRuning())
            {
                try
                {
                    this->gps.updateData();
                }
                catch (const std::exception &e)
                {
                    Debug::error(__FILE__, __LINE__, __func__, "%s\n", e.what());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
            }
        }));
}

void GpsHandler::stop()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = false;
    }
    this->th->join();
    this->th.reset();
}

void GpsHandler::access(std::function<void(const Nmea &nmea)> accessHandler)
{
    std::lock_guard<std::mutex> guard(this->mtx);
    this->gps.accessData(accessHandler);
}