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
class GsmHandler;
class WorkflowManager;
class Controller;

class TscDeliveryHandler
{
private:
    bool isRun;
    bool isSendMarriageCode;
    std::time_t lastHeartBeatSent;
    std::string localDatabasePath;
    std::unique_ptr<std::thread> th;
    ASA &asa;
    TJS &tjs;
    GsmHandler &gsm;
    WorkflowManager &workflow;
    Controller &controler;
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
    TscDeliveryHandler(ASA &asa, TJS &tjs, GsmHandler &gsm, WorkflowManager &workflow, Controller &controler);
    ~TscDeliveryHandler();

    void setTransactionLocalDatabase(const std::string &filePath);

    bool isRuning() const;

    void begin();
    void stop();

    const std::string &getLocalDatabasePath() const;

    static bool waitFor(int timeoutMs);
    static void signal();
};

#endif