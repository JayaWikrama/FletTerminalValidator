#ifndef __CONTROLLER_HPP__
#define __CONTROLLER_HPP__

#include <thread>
#include <mutex>
#include <functional>
#include <memory>

#include "error-code.hpp"
#include "uncomplete-write-handler.hpp"

#ifndef FTV_WORKING_DIRECTORY
#define FTV_WORKING_DIRECTORY "."
#endif

#define CONFIG_DIRECTORY FTV_WORKING_DIRECTORY "/config"
#define DATA_DIRECTORY FTV_WORKING_DIRECTORY "/data"
#define LOG_DIRECTORY FTV_WORKING_DIRECTORY "/log"

#define COUNTER_DATA_DIRECTORY DATA_DIRECTORY "/counter"
#define MAIN_APP_LOG_DIRECTORY LOG_DIRECTORY "/main"
#define EPAYMENT_MODULE_LOG_DIRECTORY LOG_DIRECTORY "/epayment"

#define TRANSACTION_DATABASE DATA_DIRECTORY "/transaction.db"
#define MAIN_APP_LOG_FILE "main_app"
#define COMM_CONFIG_FILE CONFIG_DIRECTORY "/communication.json"
#define PROVISION_CONFIG_FILE CONFIG_DIRECTORY "/provision.json"

class Gui;
class Epayment;
class SAMHandler;
class WorkflowManager;
class TransactionRules;
class CardData;
class Duration;
class Counter;
class GpsHandler;
class GsmHandler;
class ASA;

class Controller
{
private:
    bool isRun;
    Epayment &epayment;
    WorkflowManager &workflow;
    Gui &gui;
    GpsHandler &gpsHandler;
    GsmHandler &gsmHandler;
    SAMHandler &samHandler;
    ASA &asa;
    UncompleteWriteHandler uncompleWriteHandler;
    std::unique_ptr<std::thread> th;
    std::unique_ptr<Counter> counter;
    mutable std::mutex mtx;

    bool processAttachedCard(Duration &duration);
    bool storeTransaction(bool isTapIn,
                          bool isDeduct,
                          const std::time_t time,
                          const int lastBalance,
                          const CardData &refUserData,
                          const TransactionRules &rules,
                          const std::string &transcodeStr,
                          Duration &duration);

    bool storeErrorTransactionOnReadFailed(const std::time_t time, Duration &duration, const ErrorCode::Code &desc);

    bool storeErrorTransactionOnReadSuccess(bool isTapIn,
                                            bool isDeduct,
                                            const std::time_t time,
                                            const int lastBalance,
                                            const CardData &refUserData,
                                            const TransactionRules &rules,
                                            Duration &duration,
                                            const ErrorCode::Code &desc);

    bool storeErrorInsufficientBalance(bool isTapIn,
                                       bool isDeduct,
                                       const int lastBalance,
                                       const CardData &refUserData,
                                       const TransactionRules &rules,
                                       Duration &duration);

    bool storeErrorGetBalance(bool isTapIn,
                              bool isDeduct,
                              const CardData &refUserData,
                              const TransactionRules &rules,
                              Duration &duration);

    bool storeErrorPurchaseBalance(bool isTapIn,
                                   bool isDeduct,
                                   const CardData &refUserData,
                                   const TransactionRules &rules,
                                   Duration &duration);

    bool storeErrorWriteUserData(bool isTapIn,
                                 bool isDeduct,
                                 const int lastBalance,
                                 const CardData &refUserData,
                                 const TransactionRules &rules,
                                 Duration &duration);

    bool storeErrorBlockingTime(const CardData &refUserData,
                                const TransactionRules &rules,
                                Duration &duration);

    bool storeErrorFreeServiceExpired(const CardData &refUserData,
                                      const TransactionRules &rules,
                                      Duration &duration);

    void routine();
    void reloadCounter();

public:
    Controller(Epayment &epayment,
               WorkflowManager &workflow,
               GpsHandler &gpsHandler,
               GsmHandler &gsmHandler,
               SAMHandler &samHandler,
               ASA &asa,
               Gui &gui);
    ~Controller();

    bool isRuning();

    void begin(std::function<void(SAMHandler &samHandler, WorkflowManager &workflow, ASA &asa, Gui &gui)> preSetup);
    void stop();

    void accessCounter(std::function<void(Counter &counter)> handler);
};

#endif