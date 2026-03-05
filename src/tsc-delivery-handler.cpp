#include <ctime>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

#include "error-code.hpp"
#include "gsm-handler.hpp"
#include "controller.hpp"
#include "counter.hpp"
#include "directory-cleaner.hpp"
#include "tscdata/include/transaction-data.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"
#include "communication/include/asa.hpp"
#include "communication/include/tjs.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "epayment/include/epayment.hpp"
#include "gui/include/gui.hpp"
#include "utils/include/debug.hpp"
#include "utils/include/time.hpp"

#include "tsc-delivery-handler.hpp"

#define SENT_INTERVAL 10
#define HEART_BEAT_INTERVAL 60
#define WAIT_INIT_TIMEOUT 60

bool TscDeliveryHandler::isReady = false;
std::condition_variable TscDeliveryHandler::condition;
std::mutex TscDeliveryHandler::conditionMtx;

void TscDeliveryHandler::checkMarriageCodeUpdateRequirement()
{
    this->isSendMarriageCode = false;
    this->controler.accessEpayment(
        [this](Epayment &epayment)
        {
            SAM *samBNI = epayment.getSAM(SAM::SAM_TYPE_BNI);
            if (samBNI != nullptr)
            {
                if (samBNI->getActiveMarriageCode().compare(samBNI->getMarriageCodeOverlay()) != 0)
                {
                    Debug::info(__FILE__, __LINE__, __func__, "BNI SAM Marriage Code update required\n");
                    this->isSendMarriageCode = true;
                }
            }
        });
}

bool TscDeliveryHandler::sendDataToMainServer()
{
    TransactionData tsc;
    int ret = 0;

    Debug::info(__FILE__, __LINE__, __func__, "query data\n");
    ret = this->localTscDatabase.queryPendingLog(tsc, true);
    if (ret)
    {
        Debug::warning(__FILE__, __LINE__, __func__, "query data failed with return code: %d\n", ret);
        return false;
    }

    nlohmann::json data;
    bool result = false;
    bool isTscSuccess = tsc.toJson(data, "1.0.0");
    if (isTscSuccess)
    {
        const Provision &provision = this->workflow.getProvision();
        result = this->asa.sendTransaction(data,
                                           provision.getData().getBusinessEntityProfile().getId(),
                                           provision.getData().getCode(),
                                           (provision.getData().getDeviceModel().getName().empty() ? provision.getData().getDeviceVersion()
                                                                                                   : provision.getData().getDeviceModel().getName()),
                                           provision.getData().getDeviceVersion(),
                                           provision.getData().getTransportationType());
        if (result)
        {
            Debug::info(__FILE__, __LINE__, __func__, "success to send data [success transaction] %s\n", tsc.getUUID().c_str());
            this->localTscDatabase.updateSuccessToSentToMainServer(tsc.getUUID());
            Debug::info(__FILE__, __LINE__, __func__, "status for %s updated\n", tsc.getUUID().c_str());
            controler.accessCounter(
                [](Counter &counter)
                {
                    counter.incSent();
                    counter.storeSN();
                    Debug::info(__FILE__, __LINE__, __func__, "counter updated\n");
                });
        }
    }
    else
    {
        const Provision &provision = this->workflow.getProvision();

        data["desc_notifications"] = ErrorCode::description(data["desc"].get<std::string>());
        data["device_id"] = provision.getData().getDeviceId();

        Debug::info(__FILE__, __LINE__, __func__, "TSC Data to Post: %s\n", data.dump(2).c_str());

        result = asa.sendTransactionFailed(data);
        if (result)
        {
            Debug::info(__FILE__, __LINE__, __func__, "success to send data [failed transaction] %s\n", tsc.getUUID().c_str());
            this->localTscDatabase.updateSuccessToSentToMainServer(tsc.getUUID());
        }
    }
    return result;
}

bool TscDeliveryHandler::sendDataToSecondaryServer()
{
    TransactionData tsc;
    int ret = 0;

    Debug::info(__FILE__, __LINE__, __func__, "query data\n");
    ret = this->localTscDatabase.queryPendingLog(tsc, false);
    if (ret)
    {
        Debug::warning(__FILE__, __LINE__, __func__, "query data failed with return code: %d\n", ret);
        return false;
    }

    nlohmann::json data;
    tsc.toJsonToTJApi(data);
    const Provision &provision = this->workflow.getProvision();
    bool result = this->tjs.sendTransaction(data);
    if (result)
    {
        Debug::info(__FILE__, __LINE__, __func__, "success to send data %s\n", tsc.getUUID().c_str());
        this->localTscDatabase.updateSuccessToSentToSecondServer(tsc.getUUID());
    }
    return result;
}

bool TscDeliveryHandler::sendHeartBeat()
{
    std::time_t currentTime = std::time(nullptr);
    if (std::abs(difftime(currentTime, this->lastHeartBeatSent)) > HEART_BEAT_INTERVAL)
    {
        if (this->asa.sendHeartBeat())
        {
            if (this->asa.isMD5ProvisionMismatch())
            {
                this->controler.stop();
                int64_t tryDelay = 3;
                while (this->asa.provision(PROVISION_CONFIG_FILE) == false)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(tryDelay));
                    if (tryDelay < 30)
                        tryDelay += 3;
                    else
                        tryDelay = 300;
                }
                Debug::info(__FILE__, __LINE__, __func__, "reboot device\n");
                Debug::moveLogHistoryToFile();
                for (int i = 11; i > 0; i--)
                {
                    this->gui.message.show({"",
                                            "REBOOT IN",
                                            std::to_string(i - 1),
                                            (i > 2 ? "SECONDS" : "SECOND"),
                                            ""});
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                system("reboot");
            }

            bool isNoLogOperationAvailable = false;
            {
                std::lock_guard<std::mutex> guard(this->mtx);
                isNoLogOperationAvailable = (this->sendLogStatus == TscDeliveryHandler::SendLogStatus::NONE);
            }

            if (isNoLogOperationAvailable)
            {
                if (this->asa.isNeedToSendLog())
                {
                    std::tm tmp{};
                    TimeUtils::fromEpoch(&tmp, std::time(nullptr));
                    std::string datetimeRFC_3339 = TimeUtils::format(&tmp, TimeUtils::TIME_FORMAT_RFC_3339);
                    this->zipLogFileName = std::string(TMP_DIRECTORY) + "/" + this->asa.getSerialNumber() + "-" + datetimeRFC_3339 + ".zip";
                    std::string command = "zip -r '" + this->zipLogFileName + "' " + std::string(LOG_DIRECTORY);
                    Debug::info(__FILE__, __LINE__, __func__, "command: \"%s\"\n", command.c_str());

                    if (mkdir(TMP_DIRECTORY, 0777) == 0)
                        Debug::info(__FILE__, __LINE__, __func__, "create directory \"%s\" success\n", TMP_DIRECTORY);
                    else if (errno == EEXIST)
                        Debug::info(__FILE__, __LINE__, __func__, "directory \"%s\" already exist\n", TMP_DIRECTORY);

                    if (system(command.c_str()) == 0)
                    {
                        {
                            std::lock_guard<std::mutex> guard(this->mtx);
                            this->sendLogStatus = TscDeliveryHandler::SendLogStatus::PROCESS;
                        }
                        this->thSendLog.reset(new std::thread(
                            [this]()
                            {
                                ASA asaLog(COMM_CONFIG_FILE);
                                asaLog.load();
                                asaLog.setToken(this->asa.getToken());
                                if (asaLog.sendLog(this->zipLogFileName))
                                {
                                    Debug::info(__FILE__, __LINE__, __func__, "send log \"%s\" success\n", this->zipLogFileName.c_str());
                                    std::lock_guard<std::mutex> guard(this->mtx);
                                    this->sendLogStatus = TscDeliveryHandler::SendLogStatus::DONE;
                                }
                                else
                                {
                                    Debug::error(__FILE__, __LINE__, __func__, "send log \"%s\" failed\n", this->zipLogFileName.c_str());
                                    std::lock_guard<std::mutex> guard(this->mtx);
                                    this->sendLogStatus = TscDeliveryHandler::SendLogStatus::FAILED;
                                }
                                std::lock_guard<std::mutex> guard(this->mtx);
                                DirectoryCleaner dirClean(TMP_DIRECTORY, ".zip");
                                if (dirClean.execute() == false)
                                {
                                    Debug::error(__FILE__, __LINE__, __func__, "failed to clean \".zip\" from %s\n", TMP_DIRECTORY);
                                }
                            }));
                    }
                    else
                    {
                        Debug::error(__FILE__, __LINE__, __func__, "failed to generate \"%s\"\n", this->zipLogFileName.c_str());
                    }
                }
                if (this->asa.isNeedToClearLog())
                {
                    std::lock_guard<std::mutex> guard(this->mtx);
                    if (this->sendLogStatus == TscDeliveryHandler::SendLogStatus::NONE)
                    {
                        DirectoryCleaner logClean(LOG_DIRECTORY, ".log");
                        if (logClean.execute("_") == false)
                        {
                            Debug::error(__FILE__, __LINE__, __func__, "failed to clean log\n");
                        }
                        else
                        {
                            Debug::info(__FILE__, __LINE__, __func__, "clean log success\n", this->zipLogFileName.c_str());
                            this->scheduleCleanLog = false;
                        }
                        DirectoryCleaner xzClean(LOG_DIRECTORY, ".xz");
                        xzClean.execute();
                    }
                    else
                        this->scheduleCleanLog = true;
                }
            }
        }
        this->lastHeartBeatSent = currentTime;
    }
    return true;
}

bool TscDeliveryHandler::sendMarriageCodeIfNeed()
{
    if (this->isSendMarriageCode)
    {
        std::string mid;
        std::string tid;
        std::string marriageCode;
        this->controler.accessEpayment(
            [&mid, &tid, &marriageCode](Epayment &epayment)
            {
                SAM *samBNI = epayment.getSAM(SAM::SAM_TYPE_BNI);
                if (samBNI != nullptr)
                {
                    mid = samBNI->getMID();
                    tid = samBNI->getTID();
                    marriageCode = samBNI->getActiveMarriageCode();
                }
            });
        if (marriageCode.empty() == false)
        {
            if (this->asa.sendMarriageCode(marriageCode, mid, tid))
            {
                this->isSendMarriageCode = false;
                Debug::info(__FILE__, __LINE__, __func__, "success to update marriage code data\n");
            }
            else
                Debug::error(__FILE__, __LINE__, __func__, "failed to update marriage code data\n");
        }
    }
    return !(this->isSendMarriageCode);
}

TscDeliveryHandler::TscDeliveryHandler(ASA &asa,
                                       TJS &tjs,
                                       GsmHandler &gsm,
                                       WorkflowManager &workflow,
                                       Controller &controler,
                                       Sqlite3Transaction &localTscDatabase,
                                       Gui &gui) : isRun(false),
                                                   isSendMarriageCode(false),
                                                   lastHeartBeatSent(0),
                                                   sendLogStatus(TscDeliveryHandler::SendLogStatus::NONE),
                                                   zipLogFileName(),
                                                   th(),
                                                   thSendLog(),
                                                   asa(asa),
                                                   tjs(tjs),
                                                   gui(gui),
                                                   gsm(gsm),
                                                   workflow(workflow),
                                                   controler(controler),
                                                   localTscDatabase(localTscDatabase),
                                                   mtx() {}

TscDeliveryHandler::~TscDeliveryHandler()
{
    this->stop();
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
            TscDeliveryHandler::waitFor(WAIT_INIT_TIMEOUT * 1000);
            this->checkMarriageCodeUpdateRequirement();
            while (this->isRuning())
            {
                if (this->gsm.isConnected() == false || this->asa.getToken().empty())
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                {
                    std::lock_guard<std::mutex> guard(this->mtx);
                    if (this->sendLogStatus == TscDeliveryHandler::SendLogStatus::DONE ||
                        this->sendLogStatus == TscDeliveryHandler::SendLogStatus::DONE)
                    {
                        this->thSendLog->join();
                        this->thSendLog.reset();
                        this->sendLogStatus = TscDeliveryHandler::SendLogStatus::NONE;
                    }
                    if (this->sendLogStatus == TscDeliveryHandler::SendLogStatus::NONE && this->scheduleCleanLog)
                    {
                        DirectoryCleaner logClean(LOG_DIRECTORY, ".log");
                        if (logClean.execute("_") == false)
                        {
                            Debug::error(__FILE__, __LINE__, __func__, "failed to clean log\n");
                        }
                        else
                        {
                            Debug::info(__FILE__, __LINE__, __func__, "clean log success\n", this->zipLogFileName.c_str());
                            this->scheduleCleanLog = false;
                        }
                        DirectoryCleaner xzClean(LOG_DIRECTORY, ".xz");
                        xzClean.execute();
                    }
                }

                this->sendMarriageCodeIfNeed();
                this->sendDataToMainServer();

                if (this->tjs.getToken().empty() != true)
                    this->sendDataToSecondaryServer();

                this->sendHeartBeat();

                TscDeliveryHandler::waitFor(SENT_INTERVAL * 1000);
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

bool TscDeliveryHandler::waitFor(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(TscDeliveryHandler::conditionMtx);

    bool result = TscDeliveryHandler::condition.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        []()
        { return TscDeliveryHandler::isReady; });

    if (result)
    {
        TscDeliveryHandler::isReady = false;
    }

    return result;
}

void TscDeliveryHandler::signal()
{
    {
        std::lock_guard<std::mutex> lock(TscDeliveryHandler::conditionMtx);
        TscDeliveryHandler::isReady = true;
    }

    TscDeliveryHandler::condition.notify_one();
}