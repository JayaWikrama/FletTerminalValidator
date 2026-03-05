#ifndef __TSC_DELIVERY_HANDLER_HPP__
#define __TSC_DELIVERY_HANDLER_HPP__

#include <ctime>
#include <functional>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>

class ASA;
class TJS;
class Gui;
class GsmHandler;
class WorkflowManager;
class Controller;
class Sqlite3Transaction;

class TscDeliveryHandler
{
public:
    enum class SendLogStatus : unsigned char
    {
        NONE = 0x00,
        PROCESS = 0x01,
        DONE = 0x02,
        FAILED = 0x03
    };

private:
    bool isRun;
    bool isSendMarriageCode;
    bool scheduleCleanLog;
    std::time_t lastHeartBeatSent;
    SendLogStatus sendLogStatus;
    std::string zipLogFileName;
    std::unique_ptr<std::thread> th;
    std::unique_ptr<std::thread> thSendLog;
    ASA &asa;
    TJS &tjs;
    Gui &gui;
    GsmHandler &gsm;
    WorkflowManager &workflow;
    Controller &controler;
    Sqlite3Transaction &localTscDatabase;
    mutable std::mutex mtx;

    static bool isReady;
    static std::condition_variable condition;
    static std::mutex conditionMtx;

    void checkMarriageCodeUpdateRequirement();

    bool sendDataToMainServer();
    bool sendDataToSecondaryServer();
    bool sendHeartBeat();
    bool sendMarriageCodeIfNeed();

public:
    TscDeliveryHandler(ASA &asa,
                       TJS &tjs,
                       GsmHandler &gsm,
                       WorkflowManager &workflow,
                       Controller &controler,
                       Sqlite3Transaction &localTscDatabase,
                       Gui &gui);
    ~TscDeliveryHandler();

    bool isRuning() const;

    void begin();
    void stop();

    static bool waitFor(int timeoutMs);
    static void signal();
};

#endif