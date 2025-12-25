#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include "controller.hpp"
#include "ui-helper.hpp"
#include "duration.hpp"
#include "gui/include/gui.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"

#include "utils/include/debug.hpp"

bool Controller::processAttachedCard(Duration &duration)
{
    std::array<unsigned char, 64> userData;
    unsigned short interop = 0;
    std::time_t expireOn = 0;

    UIHelper::processingCard(this->gui);

    unsigned long long cardNumber = epayment.getCardNumber();
    duration.checkPoint("get card number");
    this->gui.labelCardNumber.setPAN(cardNumber, "", true);
    Debug::info(__FILE__, __LINE__, __func__, "card number: %016llu\n", cardNumber);

    if (epayment.readUserData<64>(userData) == false)
    {
        duration.checkPoint("read user data failed");
        UIHelper::failedToReadCard(this->gui, "1004");
        return false;
    }
    duration.checkPoint("read user data");
    epayment.getFreeServiceParam(interop, expireOn);

#ifdef __HARDCODE_EXPIRED_ON
    expireOn = __HARDCODE_EXPIRED_ON;
#endif

    WorkflowManager &work = this->workflow;
    bool result = false;

    work.validate(cardNumber, 0, userData, interop, expireOn)
        .onPinalty(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    duration.checkPoint("deduct pinalty success");
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
                        duration.checkPoint("write user data success");
                        UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                        if (refUserData.isCardOKOTrip())
                            type = UIHelper::TariffType::JAKLINGKO;
                        else if (refUserData.isCardFreeServices())
                            type = UIHelper::TariffType::FREE;

                        UIHelper::successTapInWithDeduct(this->gui,
                                                         rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                         rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                         epayment.getLastBalance(),
                                                         type,
                                                         refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", epayment.getLastBalance());
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", epayment.getTranscodeUTF8());
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    duration.checkPoint("deduct pinalty failed");
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            return;
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(epayment.getLastStatus())));
                }
            })
        .onTapInWithDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    duration.checkPoint("deduct on tap in success");
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
                        duration.checkPoint("write user data success");
                        UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                        if (refUserData.isCardOKOTrip())
                            type = UIHelper::TariffType::JAKLINGKO;
                        else if (refUserData.isCardFreeServices())
                            type = UIHelper::TariffType::FREE;

                        UIHelper::successTapInWithDeduct(this->gui,
                                                         rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                         rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                         epayment.getLastBalance(),
                                                         type,
                                                         refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", epayment.getLastBalance());
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", epayment.getTranscodeUTF8());
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    duration.checkPoint("deduct on tap in failed");
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            return;
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(epayment.getLastStatus())));
                }
            })
        .onTapOutWithoutDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                result = this->epayment.writeUserData<64>(toWrite, userData);
                if (result)
                {
                    duration.checkPoint("write user data success");
                    cardBalance = this->epayment.getBalance();
                    duration.checkPoint("get balance operation on tap out without deduct");
                    result = (cardBalance >= 0);
                    Debug::info(__FILE__, __LINE__, __func__, "balance: %u\n", cardBalance);

                    UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                    if (refUserData.isCardOKOTrip())
                        type = UIHelper::TariffType::JAKLINGKO;
                    else if (refUserData.isCardFreeServices())
                        type = UIHelper::TariffType::FREE;

                    UIHelper::successTapOutWithoutDeduct(this->gui, cardBalance, type, refUserData.freeService.expireOn);
                }
                else
                {
                    duration.checkPoint("write user data failed");
                    UIHelper::failedToWriteCard(this->gui, "1004");
                }
            })
        .onTapInWithoutDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                result = this->epayment.writeUserData<64>(toWrite, userData);
                if (result)
                {
                    duration.checkPoint("write user data success");
                    cardBalance = this->epayment.getBalance();
                    duration.checkPoint("get balance operation on tap in without deduct");
                    result = (cardBalance >= 0);
                    Debug::info(__FILE__, __LINE__, __func__, "balance: %u\n", cardBalance);

                    UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                    if (refUserData.isCardOKOTrip())
                        type = UIHelper::TariffType::JAKLINGKO;
                    else if (refUserData.isCardFreeServices())
                        type = UIHelper::TariffType::FREE;

                    UIHelper::successTapInWithoutDeduct(this->gui, cardBalance, type, refUserData.freeService.expireOn);
                }
                else
                {
                    duration.checkPoint("write user data failed");
                    UIHelper::failedToWriteCard(this->gui, "1004");
                }
            })
        .onTapOutWithDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    duration.checkPoint("deduct on tap out success");
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
                        duration.checkPoint("write user data success");
                        UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                        if (refUserData.isCardOKOTrip())
                            type = UIHelper::TariffType::JAKLINGKO;
                        else if (refUserData.isCardFreeServices())
                            type = UIHelper::TariffType::FREE;

                        UIHelper::successTapOutWithDeduct(this->gui,
                                                          rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                          rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()),
                                                          epayment.getLastBalance(),
                                                          type,
                                                          refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", epayment.getLastBalance());
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", epayment.getTranscodeUTF8());
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    duration.checkPoint("deduct on tap out failed");
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            return;
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(epayment.getLastStatus())));
                }
            })
        .onFreeServiceExpired(
            [this, &result](const CardData &refUserData, const std::array<unsigned char, 64> &originData)
            {
                UIHelper::freeServiceExpired(this->gui, refUserData.freeService.expireOn);
            })
        .onBlocking(
            [this, &result](const CardData &refUserData, const std::array<unsigned char, 64> &originData)
            {
                UIHelper::blockingTime(this->gui);
            })
        .onInvalid(
            [this](const std::array<unsigned char, 64> &userData)
            {
                Debug::error(__FILE__, __LINE__, __func__, "invalid user data\n");
            });

    return result;
}

void Controller::routine()
{
    if (this->epayment.selectAttachedCard())
    {
        bool result = false;
        try
        {
            Duration duration("transaction");
            result = this->processAttachedCard(duration);
        }
        catch (const std::exception &e)
        {
            Debug::info(__FILE__, __LINE__, __func__, "catch: %s\n", e.what());
        }
        if (result)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
        UIHelper::reset(this->gui, 1);
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

Controller::Controller(Epayment &epayment, WorkflowManager &workflow, Gui &gui) : isRun(false),
                                                                                  epayment(epayment),
                                                                                  workflow(workflow),
                                                                                  gui(gui),
                                                                                  th(),
                                                                                  mtx() {}

Controller::~Controller()
{
    this->stop();
}

void Controller::setup(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> handler)
{
    std::lock_guard<std::mutex> guard(this->mtx);
    handler(this->epayment, this->workflow, this->gui);
}

bool Controller::isRuning()
{
    std::lock_guard<std::mutex> guard(this->mtx);
    return this->isRun;
}

void Controller::begin(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> preSetup)
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = true;
    }
    this->th.reset(new std::thread(
        [this, preSetup]()
        {
            this->gui.waitObjectReady();
            {
                std::lock_guard<std::mutex> guard(this->mtx);
                preSetup(this->epayment, this->workflow, this->gui);
            }
            while (this->isRuning())
            {
                this->routine();
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
            }
        }));
}

void Controller::stop()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = false;
    }
    this->th->join();
    this->th.reset();
}