#ifndef __TSC_DELIVERY_HANDLER_HPP__
#define __TSC_DELIVERY_HANDLER_HPP__

#include <functional>
#include <thread>
#include <string>
#include <mutex>

class ASA;
class GsmHandler;
class WorkflowManager;
class Controller;

class TscDeliveryHandler
{
private:
    bool isRun;
    int successCounter;
    std::string localDatabasePath;
    std::unique_ptr<std::thread> th;
    ASA &asa;
    GsmHandler &gsm;
    WorkflowManager &workflow;
    Controller &controler;
    mutable std::mutex mtx;

public:
    TscDeliveryHandler(ASA &asa, GsmHandler &gsm, WorkflowManager &workflow, Controller &controller);
    ~TscDeliveryHandler();

    void setTransactionLocalDatabase(const std::string &filePath);

    bool isSuccessCounterAvailable() const;
    void accessSuccessCounter(std::function<void(int &counter)> handler);

    bool isRuning() const;

    void begin();
    void stop();

    const std::string &getLocalDatabasePath() const;
};

#endif