#include <ctime>

#include "error-code.hpp"
#include "gsm-handler.hpp"
#include "tscdata/include/transaction-data.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"
#include "communication/include/asa.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "utils/include/debug.hpp"

#include "tsc-delivery-handler.hpp"

#define SENT_INTERVAL 10

TscDeliveryHandler::TscDeliveryHandler(ASA &asa, GsmHandler &gsm, WorkflowManager &workflow) : isRun(false),
                                                                                               successCounter(0),
                                                                                               localDatabasePath(),
                                                                                               th(),
                                                                                               asa(asa),
                                                                                               gsm(gsm),
                                                                                               workflow(workflow),
                                                                                               mtx() {}

TscDeliveryHandler::~TscDeliveryHandler()
{
    this->stop();
}

void TscDeliveryHandler::setTransactionLocalDatabase(const std::string &filePath)
{
    std::lock_guard<std::mutex> guard(this->mtx);
    this->localDatabasePath = filePath;
}

bool TscDeliveryHandler::isSuccessCounterAvailable() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return (this->successCounter > 0);
}

void TscDeliveryHandler::accessSuccessCounter(std::function<void(int &counter)> handler)
{
    std::lock_guard<std::mutex> guard(this->mtx);
    handler(this->successCounter);
}

bool TscDeliveryHandler::isRuning() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->isRun;
}

void TscDeliveryHandler::begin()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = true;
    }
    this->th.reset(new std::thread(
        [this]()
        {
            std::time_t lastCheck = std::time(NULL) - SENT_INTERVAL;
            std::time_t currentTime = 0;
            int ret = 0;
            while (this->isRuning())
            {
                if (this->gsm.isConnected() == false && 0)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    continue;
                }
                currentTime = std::time(nullptr);

                TransactionData tsc;
                Sqlite3Transaction tscdb(this->getLocalDatabasePath());

                Debug::info(__FILE__, __LINE__, __func__, "query data from: %s\n", this->getLocalDatabasePath().c_str());
                ret = tscdb.queryPendingLog(tsc, true);
                if (ret)
                {
                    Debug::warning(__FILE__, __LINE__, __func__, "query data failed with return code: %d\n", ret);
                    std::this_thread::sleep_for(std::chrono::seconds(SENT_INTERVAL));
                    continue;
                }

                nlohmann::json data;
                bool isTscSuccess = tsc.toJson(data, "1.0.0");
                if (isTscSuccess)
                {
                    Debug::info(__FILE__, __LINE__, __func__, "TSC Data to Post: %s\n", data.dump(2).c_str());
                    const Provision &provision = this->workflow.getProvision();
                    bool result = asa.sendTransaction(data,
                                                      provision.getData().getBusinessEntityProfile().getId(),
                                                      provision.getData().getCode(),
                                                      (provision.getData().getDeviceModel().getName().empty() ? provision.getData().getDeviceVersion()
                                                                                                              : provision.getData().getDeviceModel().getName()),
                                                      provision.getData().getDeviceVersion(),
                                                      provision.getData().getLocationType());
                    if (result)
                    {
                        Debug::info(__FILE__, __LINE__, __func__, "success to send data [success transaction] %s\n", tsc.getUUID().c_str());
                        tscdb.updateSuccessToSentToMainServer(tsc.getUUID());
                    }
                }
                else
                {
                    const Provision &provision = this->workflow.getProvision();

                    data["desc_notifications"] = ErrorCode::description(data["desc"].get<std::string>());
                    data["device_id"] = provision.getData().getDeviceId();

                    Debug::info(__FILE__, __LINE__, __func__, "TSC Data to Post: %s\n", data.dump(2).c_str());

                    bool result = asa.sendTransactionFailed(data);
                    if (result)
                    {
                        Debug::info(__FILE__, __LINE__, __func__, "success to send data [failed transaction] %s\n", tsc.getUUID().c_str());
                        tscdb.updateSuccessToSentToMainServer(tsc.getUUID());
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(SENT_INTERVAL));
            }
        }));
}

void TscDeliveryHandler::stop()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = false;
    }
    this->th->join();
    this->th.reset();
}

const std::string &TscDeliveryHandler::getLocalDatabasePath() const
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->localDatabasePath;
}