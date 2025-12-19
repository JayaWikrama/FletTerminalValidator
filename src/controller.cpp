#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include "controller.hpp"
#include "ui-helper.hpp"
#include "gui/include/gui.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"

bool Controller::processAttachedCard()
{
    std::array<unsigned char, 64> userData;
    unsigned short interop = 0;
    std::time_t expireOn = 0;

    UIHelper::processingCard(this->gui);

    unsigned long long cardNumber = epayment.getCardNumber();
    this->gui.labelCardNumber.setPAN(cardNumber, "", true);
    std::cout << "Card Number: " << cardNumber << std::endl;

    if (epayment.readUserData<64>(userData) == false)
    {
        UIHelper::failedToReadCard(this->gui, "1004");
        return false;
    }
    epayment.getFreeServiceParam(interop, expireOn);

#ifdef __HARDCODE_EXPIRED_ON
    expireOn = __HARDCODE_EXPIRED_ON;
#endif

    WorkflowManager &work = this->workflow;
    bool result = false;

    work.validate(cardNumber, 0, userData, interop, expireOn)
        .onPinalty(
            [this, &result, &userData](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
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

                        std::cout << "Sisa Saldo: " << std::to_string(epayment.getLastBalance()) << std::endl;
                        std::cout << "Transcode: " << epayment.getTranscodeUTF8() << std::endl;
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
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
            [this, &result, &userData](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
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

                        std::cout << "Sisa Saldo: " << std::to_string(epayment.getLastBalance()) << std::endl;
                        std::cout << "Transcode: " << epayment.getTranscodeUTF8() << std::endl;
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
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
            [this, &result, &userData](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                result = this->epayment.writeUserData<64>(toWrite, userData);
                if (result)
                {
                    cardBalance = this->epayment.getBalance();
                    result = (cardBalance >= 0);
                    std::cout << "Card Balance B: " << cardBalance << std::endl;

                    UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                    if (refUserData.isCardOKOTrip())
                        type = UIHelper::TariffType::JAKLINGKO;
                    else if (refUserData.isCardFreeServices())
                        type = UIHelper::TariffType::FREE;

                    UIHelper::successTapOutWithoutDeduct(this->gui, cardBalance, type, refUserData.freeService.expireOn);
                }
                else
                {
                    UIHelper::failedToWriteCard(this->gui, "1004");
                }
            })
        .onTapInWithoutDeduct(
            [this, &result, &userData](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                result = this->epayment.writeUserData<64>(toWrite, userData);
                if (result)
                {
                    cardBalance = this->epayment.getBalance();
                    result = (cardBalance >= 0);
                    std::cout << "Card Balance A: " << cardBalance << std::endl;

                    UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                    if (refUserData.isCardOKOTrip())
                        type = UIHelper::TariffType::JAKLINGKO;
                    else if (refUserData.isCardFreeServices())
                        type = UIHelper::TariffType::FREE;

                    UIHelper::successTapInWithoutDeduct(this->gui, cardBalance, type, refUserData.freeService.expireOn);
                }
                else
                {
                    UIHelper::failedToWriteCard(this->gui, "1004");
                }
            })
        .onTapOutWithDeduct(
            [this, &result, &userData](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                this->epayment.setAmount(rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation()));
                result = this->epayment.deduct();
                if (result)
                {
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
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

                        std::cout << "Sisa Saldo: " << std::to_string(epayment.getLastBalance()) << std::endl;
                        std::cout << "Transcode: " << epayment.getTranscodeUTF8() << std::endl;
                        epayment.purchaseCommit();
                    }
                    else
                    {
                        UIHelper::failedToWriteCard(this->gui, "1004");
                    }
                }
                else
                {
                    if (epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
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
                std::cout << "Invalid card data" << std::endl;
            });

    return result;
}

void Controller::routine()
{
    if (this->epayment.selectAttachedCard())
    {
        try
        {
            if (this->processAttachedCard())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            UIHelper::reset(this->gui, 1);
        }
        catch (const std::exception &e)
        {
            std::cout << "Catch: " << e.what() << std::endl;
        }
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
                                                                                  th(nullptr),
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
    this->th = new std::thread(
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
        });
}

void Controller::stop()
{
    {
        std::lock_guard<std::mutex> guard(this->mtx);
        this->isRun = false;
    }
    this->th->join();
    delete this->th;
    this->th = nullptr;
}