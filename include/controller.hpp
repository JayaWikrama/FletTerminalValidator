#ifndef __CONTROLLER_HPP__
#define __CONTROLLER_HPP__

#include <thread>
#include <mutex>
#include <functional>
#include <memory>

#ifndef FTV_WORKING_DIRECTORY
#define FTV_WORKING_DIRECTORY "."
#endif

#define CONFIG_DIRECTORY FTV_WORKING_DIRECTORY "/config"
#define DATA_DIRECTORY FTV_WORKING_DIRECTORY "/data"
#define LOG_DIRECTORY FTV_WORKING_DIRECTORY "/log"

#define MAIN_APP_LOG_DIRECTORY LOG_DIRECTORY "/main"
#define EPAYMENT_MODULE_LOG_DIRECTORY LOG_DIRECTORY "/epayment"

#define TRANSACTION_DATABASE DATA_DIRECTORY "/transaction.db"
#define MAIN_APP_LOG_FILE "main_app"

class Gui;
class Epayment;
class WorkflowManager;
class TransactionRules;
class CardData;
class Duration;

class Controller
{
private:
    bool isRun;
    Epayment &epayment;
    WorkflowManager &workflow;
    Gui &gui;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

    bool processAttachedCard(Duration &duration);
    bool storeTransaction(bool isTapIn,
                          bool isDeduct,
                          const std::time_t time,
                          const int lastBalance,
                          const CardData &refUserData,
                          const TransactionRules &rules,
                          Duration &duration);

    void routine();

public:
    Controller(Epayment &epayment, WorkflowManager &workflow, Gui &gui);
    ~Controller();

    void setup(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> handler);

    bool isRuning();

    void begin(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> preSetup);
    void stop();
};

#endif