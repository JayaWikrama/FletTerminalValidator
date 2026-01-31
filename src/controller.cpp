#include <cstring>
#include <cstdlib>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include "counter.hpp"
#include "controller.hpp"
#include "ui-helper.hpp"
#include "duration.hpp"
#include "gui/include/gui.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "tscdata/include/transaction-data.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"

#include "utils/include/debug.hpp"

static std::string generateTimeBasedUUID(std::time_t timeValue)
{
    unsigned long long timestampMs = static_cast<unsigned long long>(timeValue) * 1000ULL;

    static std::mt19937 gen(static_cast<unsigned>(
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count()));

    std::uniform_int_distribution<unsigned long long> dist(0, UINT64_MAX);

    unsigned long long randA = dist(gen);
    unsigned long long randB = dist(gen);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // 48-bit timestamp
    ss << std::setw(12) << (timestampMs & 0xFFFFFFFFFFFFULL);
    ss << "-";

    // version 7
    ss << std::setw(4) << ((randA & 0x0FFFULL) | 0x7000);
    ss << "-";

    // variant RFC 4122
    ss << std::setw(4) << ((randA >> 12 & 0x3FFFULL) | 0x8000);
    ss << "-";

    ss << std::setw(4) << (randB & 0xFFFFULL);
    ss << "-";
    ss << std::setw(12) << ((randB >> 16) & 0xFFFFFFFFFFFFULL);

    return ss.str();
}

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

    work.validate(
            this->epayment.getBank(),
            (this->epayment.getIssuer().compare("jakcard") ? this->epayment.getIssuer() : "jakcard2"),
            cardNumber,
            999999,
            userData,
            interop,
            expireOn)
        .onPinalty(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                const unsigned int amountDeduct = rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation());
                this->epayment.setAmount(amountDeduct);
                if (amountDeduct > 0)
                {
                    result = this->epayment.deduct();
                    if (result)
                    {
                        cardBalance = this->epayment.getLastBalance();
                    }
                }
                else
                {
                    cardBalance = this->epayment.getBalance();
                    result = (cardBalance >= 0);
                }
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

                        UIHelper::successResetTapIn(this->gui,
                                                    amountDeduct,
                                                    amountDeduct,
                                                    cardBalance,
                                                    type,
                                                    refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", cardBalance);
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", amountDeduct > 0 ? this->epayment.getTranscodeUTF8() : "custom");

                        /* generate reset data */
                        this->storeTransaction(
                            false,
                            true,
                            std::time(nullptr),
                            cardBalance,
                            refUserData,
                            rules,
                            duration);

                        /* generate tap-in data */
                        this->storeTransaction(
                            true,
                            false,
                            std::time(nullptr),
                            cardBalance,
                            refUserData,
                            rules,
                            duration);

                        if (amountDeduct > 0)
                            this->epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                        this->storeErrorWriteUserData(true, false, cardBalance, refUserData, rules, duration);
                    }
                }
                else
                {
                    duration.checkPoint("deduct pinalty failed");
                    if (this->epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            this->storeErrorInsufficientBalance(false, true, cardBalance, refUserData, rules, duration);
                            return;
                        }
                        else
                        {
                            this->storeErrorGetBalance(false, true, refUserData, rules, duration);
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(this->epayment.getLastStatus())));
                    if (amountDeduct > 0)
                        this->storeErrorPurchaseBalance(false, true, refUserData, rules, duration);
                    else
                        this->storeErrorGetBalance(false, true, refUserData, rules, duration);
                }
            })
        .onTapInWithDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                const unsigned int amountDeduct = rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation());
                this->epayment.setAmount(amountDeduct);
                if (amountDeduct > 0)
                {
                    result = this->epayment.deduct();
                    if (result)
                    {
                        cardBalance = this->epayment.getLastBalance();
                    }
                }
                else
                {
                    cardBalance = this->epayment.getBalance();
                    result = (cardBalance >= 0);
                }
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
                                                         amountDeduct,
                                                         amountDeduct,
                                                         cardBalance,
                                                         type,
                                                         refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", cardBalance);
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", amountDeduct > 0 ? this->epayment.getTranscodeUTF8() : "custom");

                        this->storeTransaction(
                            true,
                            true,
                            std::time(nullptr),
                            cardBalance,
                            refUserData,
                            rules,
                            duration);

                        if (amountDeduct > 0)
                            this->epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                        this->storeErrorWriteUserData(true, true, cardBalance, refUserData, rules, duration);
                    }
                }
                else
                {
                    duration.checkPoint("deduct on tap in failed");
                    if (this->epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            this->storeErrorInsufficientBalance(true, true, cardBalance, refUserData, rules, duration);
                            return;
                        }
                        else
                        {
                            this->storeErrorGetBalance(true, true, refUserData, rules, duration);
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(this->epayment.getLastStatus())));
                    if (amountDeduct > 0)
                        this->storeErrorPurchaseBalance(true, true, refUserData, rules, duration);
                    else
                        this->storeErrorGetBalance(true, true, refUserData, rules, duration);
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

                    this->storeTransaction(
                        false,
                        false,
                        std::time(nullptr),
                        cardBalance,
                        refUserData,
                        rules,
                        duration);
                }
                else
                {
                    duration.checkPoint("write user data failed");
                    UIHelper::failedToWriteCard(this->gui, "1004");
                    this->storeErrorWriteUserData(false, false, cardBalance, refUserData, rules, duration);
                }
            })
        .onTapInWithoutDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = this->epayment.getBalance();
                duration.checkPoint("get balance operation on tap in without deduct");
                result = (cardBalance >= 0);
                Debug::info(__FILE__, __LINE__, __func__, "balance: %u\n", cardBalance);

                if (refUserData.isCardFreeServices() == false)
                {
                    const TransJakartaFare *calculatedTransJakartaFare = rules.getCalculatedFare();
                    if (calculatedTransJakartaFare)
                    {
                        Debug::info(__FILE__, __LINE__, __func__, "minimum balance: %u\n", calculatedTransJakartaFare->getTicketRules().getMinimalBalance());
                        if (result)
                        {
                            if (cardBalance < calculatedTransJakartaFare->getTicketRules().getMinimalBalance())
                            {
                                result = false;
                                Debug::error(__FILE__, __LINE__, __func__, "insufficient minimum balance\n");
                                UIHelper::insufficientMinimumBalance(this->gui, cardBalance);
                                this->storeErrorInsufficientBalance(true, false, cardBalance, refUserData, rules, duration);
                                return;
                            }
                        }
                        else
                        {
                            this->storeErrorGetBalance(true, false, refUserData, rules, duration);
                        }
                    }
                }
                if (result)
                {
                    result = this->epayment.writeUserData<64>(toWrite, userData);
                    if (result)
                    {
                        duration.checkPoint("write user data success");

                        UIHelper::TariffType type = UIHelper::TariffType::REGULER;

                        if (refUserData.isCardOKOTrip())
                            type = UIHelper::TariffType::JAKLINGKO;
                        else if (refUserData.isCardFreeServices())
                            type = UIHelper::TariffType::FREE;

                        UIHelper::successTapInWithoutDeduct(this->gui, cardBalance, type, refUserData.freeService.expireOn);

                        this->storeTransaction(
                            true,
                            false,
                            std::time(nullptr),
                            cardBalance,
                            refUserData,
                            rules,
                            duration);
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                        this->storeErrorWriteUserData(true, false, cardBalance, refUserData, rules, duration);
                    }
                }
                else
                {
                    duration.checkPoint("get balance failed");
                    UIHelper::failedToWriteCard(this->gui, "1004");
                    this->storeErrorGetBalance(true, false, refUserData, rules, duration);
                }
            })
        .onTapOutWithDeduct(
            [this, &result, &userData, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &toWrite, const TransactionRules &rules)
            {
                int cardBalance = 0;
                const unsigned int amountDeduct = rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation());
                this->epayment.setAmount(amountDeduct);
                if (amountDeduct > 0)
                {
                    result = this->epayment.deduct();
                    if (result)
                    {
                        cardBalance = this->epayment.getLastBalance();
                    }
                }
                else
                {
                    cardBalance = this->epayment.getBalance();
                    result = (cardBalance >= 0);
                }
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
                                                          amountDeduct,
                                                          amountDeduct,
                                                          cardBalance,
                                                          type,
                                                          refUserData.freeService.expireOn);

                        Debug::info(__FILE__, __LINE__, __func__, "last balance: %u\n", cardBalance);
                        Debug::info(__FILE__, __LINE__, __func__, "transcode   : %s\n", amountDeduct > 0 ? this->epayment.getTranscodeUTF8() : "custom");

                        this->storeTransaction(
                            false,
                            true,
                            std::time(nullptr),
                            cardBalance,
                            refUserData,
                            rules,
                            duration);

                        if (amountDeduct > 0)
                            this->epayment.purchaseCommit();
                    }
                    else
                    {
                        duration.checkPoint("write user data failed");
                        UIHelper::failedToWriteCard(this->gui, "1004");
                        this->storeErrorWriteUserData(false, true, cardBalance, refUserData, rules, duration);
                    }
                }
                else
                {
                    duration.checkPoint("deduct on tap out failed");
                    if (this->epayment.getLastStatus() == Epayment::CARD_OP_INSUFFICIENT_VALUE)
                    {
                        int cardBalance = this->epayment.getBalance();
                        duration.checkPoint("get balance operation");
                        if (cardBalance >= 0)
                        {
                            UIHelper::insufficientBalance(this->gui, cardBalance);
                            this->storeErrorInsufficientBalance(false, true, cardBalance, refUserData, rules, duration);
                            return;
                        }
                        else
                        {
                            this->storeErrorGetBalance(false, true, refUserData, rules, duration);
                        }
                    }
                    UIHelper::failedToDeductCard(this->gui, std::to_string(static_cast<int>(this->epayment.getLastStatus())));
                    if (amountDeduct > 0)
                        this->storeErrorPurchaseBalance(false, true, refUserData, rules, duration);
                    else
                        this->storeErrorGetBalance(false, true, refUserData, rules, duration);
                }
            })
        .onFreeServiceExpired(
            [this, &result, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &originData, const TransactionRules &rules)
            {
                UIHelper::freeServiceExpired(this->gui, refUserData.freeService.expireOn);
                this->storeErrorFreeServiceExpired(refUserData, rules, duration);
            })
        .onBlocking(
            [this, &result, &duration](const CardData &refUserData, const std::array<unsigned char, 64> &originData, const TransactionRules &rules)
            {
                UIHelper::blockingTime(this->gui);
                this->storeErrorBlockingTime(refUserData, rules, duration);
            })
        .onInvalid(
            [this](const std::array<unsigned char, 64> &userData)
            {
                Debug::error(__FILE__, __LINE__, __func__, "invalid user data\n");
            })
        .onFareNotFound(
            [this](const std::array<unsigned char, 64> &userData)
            {
                Debug::error(__FILE__, __LINE__, __func__, "fare not found\n");
                UIHelper::fareNotFound(this->gui);
            })
        .onInsufficientBalance(
            [this](const std::array<unsigned char, 64> &userData)
            {
                Debug::error(__FILE__, __LINE__, __func__, "insufficient minimum balance\n");
                UIHelper::insufficientMinimumBalance(this->gui, 0);
            });
    return result;
}

bool Controller::storeTransaction(bool isTapIn,
                                  bool isDeduct,
                                  const std::time_t time,
                                  const int lastBalance,
                                  const CardData &refUserData,
                                  const TransactionRules &rules,
                                  Duration &duration)
{
    try
    {
        if (this->counter.get() == nullptr)
            this->reloadCounter();
        else if (counter->getCycle().isSameCycle(time) == false)
            this->reloadCounter();
    }
    catch (const std::exception &e)
    {
        Debug::error(__FILE__, __LINE__, __func__, "failed to load counter: %s\n", e.what());
    }

    const unsigned int amount = rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation());
    const TransJakartaFare *transjakartaFare = rules.getCalculatedFare();

    std::string transcode = "";
    if (isDeduct)
    {
        if (amount > 0)
        {
            transcode = epayment.getTranscode();
        }
        else
        {
            transcode = this->workflow.generateZeroDeductTranscode(this->epayment.getActiveTID(),
                                                                   this->epayment.getActiveMID(),
                                                                   this->counter.get() ? this->counter->getSN() : 0);
        }
    }

    TransactionIdentity ref;
    ref.setFletCode(refUserData.getFletCode());
    ref.setTerminalId(refUserData.getTerminalId());
    ref.setTransactionTime(refUserData.getEpochTime());
    ref.setTransportationType(refUserData.getTrasportationCode());

    TransactionIdentity me(this->workflow.getIdentity());
    me.setTransactionTime(std::time(nullptr));

    CardData card = refUserData;
    card.setIssuer(this->epayment.getIssuer());
    card.setBank(this->epayment.getBank());

    TransactionData tsc(isTapIn);

    tsc.setIntegratorId(1);
    tsc.setMinimumBalance(transjakartaFare == nullptr ? 0 : transjakartaFare->getTicketRules().getMinimalBalance());
    tsc.setBalanceBeforeTransaction(isDeduct ? (lastBalance + amount) : lastBalance);
    tsc.setNormalFare(rules.getNormalFare());
    tsc.setFare(isDeduct ? amount : 0);
    tsc.setBalanceAfterTransaction(lastBalance);
    tsc.setProcessingTimeMs(duration.getTotalDurationInMs());
    tsc.setCoordinates(0.0, 0.0);
    tsc.setTransactionTime(std::time(nullptr));
    tsc.setTransactionStoredTime(std::time(nullptr));
    tsc.setUUID(generateTimeBasedUUID(tsc.getTransactionTime()));
    tsc.setMID(this->epayment.getActiveMID());
    tsc.setTID(this->epayment.getActiveTID());
    tsc.setTranscode(transcode);
    tsc.setStatus("S");
    tsc.setDescription("S");
    tsc.setTransactionInInfo(me);
    tsc.setTransactionOutInfo(ref);
    tsc.setCardData(card);

    Sqlite3Transaction tscdb(TRANSACTION_DATABASE);

    if (tscdb.insertLog(tsc) == 0)
    {
        if (this->counter.get())
        {
            Debug::info(__FILE__, __LINE__, __func__, "success to insert transaction [%d] on cycle %li\n", this->counter->getSN(), this->counter->getCycle().getCycleTime());

            Counter::Issuer &cissuer = this->counter->getIssuerByEpaymentCardType(static_cast<int>(this->epayment.getType()));
            if (isTapIn)
            {
                if (refUserData.isCardFreeServices())
                {
                    cissuer.incTapInFreeService();
                }
                else
                {
                    if (transjakartaFare)
                    {
                        if (transjakartaFare->getFareType().compare("economy") == 0)
                        {
                            cissuer.incTapInEconomy();
                        }
                        else
                        {
                            cissuer.incTapInRegular();
                        }
                    }
                    else
                    {
                        cissuer.incTapInRegular();
                    }
                }
            }
            else
            {
                cissuer.incTapOut();
            }

            if (isDeduct && amount > 0)
            {
                cissuer.incAmount(amount);
            }

            cissuer.incPending();
            cissuer.store();

            this->counter->incSN();
            this->counter->storeSN();

            UIHelper::updateCounter(this->gui, this->counter.get());
        }
        else
        {
            Debug::warning(__FILE__, __LINE__, __func__, "success to insert transaction but counter object is null\n");
        }

        return true;
    }

    Debug::error(__FILE__, __LINE__, __func__, "failed to insert transaction\n");
    return false;
}

bool Controller::storeErrorTransactionOnReadFailed(const std::time_t time, Duration &duration, const ErrorCode::Code &desc)
{
    TransactionIdentity me(this->workflow.getIdentity());
    me.setTransactionTime(std::time(nullptr));

    std::array<unsigned char, 64UL> empty{};
    CardData card;
    card.parse(empty, this->workflow.getProvision());
    card.setIssuer(this->epayment.getIssuer());
    card.setBank(this->epayment.getBank());

    TransactionData tsc(true);

    tsc.setIntegratorId(1);
    tsc.setMinimumBalance(0);
    tsc.setBalanceBeforeTransaction(0);
    tsc.setNormalFare(this->workflow.getProvision().getData().getPriceInformation().getSingleTrip().getPrice());
    tsc.setFare(0);
    tsc.setBalanceAfterTransaction(0);
    tsc.setProcessingTimeMs(duration.getTotalDurationInMs());
    tsc.setCoordinates(0.0, 0.0);
    tsc.setTransactionTime(std::time(nullptr));
    tsc.setTransactionStoredTime(std::time(nullptr));
    tsc.setUUID(generateTimeBasedUUID(tsc.getTransactionTime()));
    tsc.setMID(this->epayment.getActiveMID());
    tsc.setTID(this->epayment.getActiveTID());
    tsc.setTranscode("");
    tsc.setStatus("F");
    tsc.setDescription(ErrorCode::toString(desc));
    tsc.setTransactionInInfo(me);
    tsc.setTransactionOutInfo(me);
    tsc.setCardData(card);

    Sqlite3Transaction tscdb(TRANSACTION_DATABASE);

    if (tscdb.insertLog(tsc) == 0)
    {
        Debug::info(__FILE__, __LINE__, __func__, "success to insert invalid transaction\n");
        return true;
    }

    Debug::error(__FILE__, __LINE__, __func__, "failed to insert transaction\n");
    return false;
}

bool Controller::storeErrorTransactionOnReadSuccess(bool isTapIn,
                                                    bool isDeduct,
                                                    const std::time_t time,
                                                    const int lastBalance,
                                                    const CardData &refUserData,
                                                    const TransactionRules &rules,
                                                    Duration &duration,
                                                    const ErrorCode::Code &desc)
{
    const unsigned int amount = rules.getFinalFare(refUserData.isCardFreeServices(), refUserData.isCardOKOTrip(), refUserData.getSubsidyAccumulation());
    const TransJakartaFare *transjakartaFare = rules.getCalculatedFare();

    TransactionIdentity ref;
    ref.setFletCode(refUserData.getFletCode());
    ref.setTerminalId(refUserData.getTerminalId());
    ref.setTransactionTime(refUserData.getEpochTime());
    ref.setTransportationType(refUserData.getTrasportationCode());

    TransactionIdentity me(this->workflow.getIdentity());
    me.setTransactionTime(std::time(nullptr));

    CardData card = refUserData;
    card.setIssuer(this->epayment.getIssuer());
    card.setBank(this->epayment.getBank());

    TransactionData tsc(isTapIn);

    tsc.setIntegratorId(1);
    tsc.setMinimumBalance(transjakartaFare == nullptr ? 0 : transjakartaFare->getTicketRules().getMinimalBalance());
    tsc.setBalanceBeforeTransaction(lastBalance);
    tsc.setNormalFare(rules.getNormalFare());
    tsc.setFare(isDeduct ? amount : 0);
    tsc.setBalanceAfterTransaction(lastBalance);
    tsc.setProcessingTimeMs(duration.getTotalDurationInMs());
    tsc.setCoordinates(0.0, 0.0);
    tsc.setTransactionTime(std::time(nullptr));
    tsc.setTransactionStoredTime(std::time(nullptr));
    tsc.setUUID(generateTimeBasedUUID(tsc.getTransactionTime()));
    tsc.setMID(this->epayment.getActiveMID());
    tsc.setTID(this->epayment.getActiveTID());
    tsc.setTranscode("");
    tsc.setStatus("F");
    tsc.setDescription(ErrorCode::toString(desc));
    tsc.setTransactionInInfo(me);
    tsc.setTransactionOutInfo(ref);
    tsc.setCardData(card);

    Sqlite3Transaction tscdb(TRANSACTION_DATABASE);

    if (tscdb.insertLog(tsc) == 0)
    {
        Debug::info(__FILE__, __LINE__, __func__, "success to insert invalid transaction\n");
        return true;
    }

    Debug::error(__FILE__, __LINE__, __func__, "failed to insert transaction\n");
    return false;
}

bool Controller::storeErrorInsufficientBalance(bool isTapIn,
                                               bool isDeduct,
                                               const int lastBalance,
                                               const CardData &refUserData,
                                               const TransactionRules &rules,
                                               Duration &duration)
{
    ErrorCode::Code ecode = ErrorCode::Code::MANDIRI_D5_INSUFFICIENT_BALANCE;
    switch (this->epayment.getType())
    {
    case Card::CARD_TYPE_BRI:
        ecode = ErrorCode::Code::BRI_A3_INSUFFICIENT_BALANCE;
        break;
    case Card::CARD_TYPE_BNI:
        ecode = ErrorCode::Code::BNI_B3_INSUFFICIENT_BALANCE;
        break;
    case Card::CARD_TYPE_BCA:
        ecode = ErrorCode::Code::BCA_E5_INSUFFICIENT_BALANCE;
        break;
    case Card::CARD_TYPE_DKI:
        ecode = ErrorCode::Code::DKI_C6_INSUFFICIENT_BALANCE;
        break;
    }
    this->storeErrorTransactionOnReadSuccess(isTapIn,
                                             isDeduct,
                                             std::time(nullptr),
                                             lastBalance,
                                             refUserData,
                                             rules,
                                             duration,
                                             ecode);
}

bool Controller::storeErrorGetBalance(bool isTapIn,
                                      bool isDeduct,
                                      const CardData &refUserData,
                                      const TransactionRules &rules,
                                      Duration &duration)
{
    ErrorCode::Code ecode = ErrorCode::Code::MANDIRI_D3_BALANCE_CHECK_EXCEPTION;
    switch (this->epayment.getType())
    {
    case Card::CARD_TYPE_BRI:
        ecode = ErrorCode::Code::BRI_AA_BALANCE_CHECK_EXCEPTION;
        break;
    case Card::CARD_TYPE_BNI:
        ecode = ErrorCode::Code::BNI_BA_BALANCE_CHECK_EXCEPTION;
        break;
    case Card::CARD_TYPE_BCA:
        ecode = ErrorCode::Code::BCA_E3_BALANCE_CHECK_EXCEPTION;
        break;
    case Card::CARD_TYPE_DKI:
        ecode = ErrorCode::Code::DKI_C3_BALANCE_CHECK_EXCEPTION;
        break;
    }
    this->storeErrorTransactionOnReadSuccess(isTapIn,
                                             isDeduct,
                                             std::time(nullptr),
                                             0,
                                             refUserData,
                                             rules,
                                             duration,
                                             ecode);
}

bool Controller::storeErrorPurchaseBalance(bool isTapIn,
                                           bool isDeduct,
                                           const CardData &refUserData,
                                           const TransactionRules &rules,
                                           Duration &duration)
{
    this->storeErrorTransactionOnReadSuccess(isTapIn,
                                             isDeduct,
                                             std::time(nullptr),
                                             0,
                                             refUserData,
                                             rules,
                                             duration,
                                             ErrorCode::Code::GENERAL_F6_DEBIT_DEVICE_LOST_CONTACT);
}

bool Controller::storeErrorWriteUserData(bool isTapIn,
                                         bool isDeduct,
                                         const int lastBalance,
                                         const CardData &refUserData,
                                         const TransactionRules &rules,
                                         Duration &duration)
{
    ErrorCode::Code ecode = ErrorCode::Code::MANDIRI_D9_WRITE_BLOCK_EXCEPTION;
    switch (this->epayment.getType())
    {
    case Card::CARD_TYPE_BRI:
        ecode = ErrorCode::Code::BRI_A7_WRITE_BLOCK_EXCEPTION;
        break;
    case Card::CARD_TYPE_BNI:
        ecode = ErrorCode::Code::BNI_B8_WRITE_BLOCK_EXCEPTION;
        break;
    case Card::CARD_TYPE_BCA:
        ecode = ErrorCode::Code::BCA_E9_WRITE_BLOCK_EXCEPTION;
        break;
    case Card::CARD_TYPE_DKI:
        ecode = ErrorCode::Code::DKI_CB_WRITE_BLOCK_EXCEPTION;
        break;
    }
    this->storeErrorTransactionOnReadSuccess(isTapIn,
                                             isDeduct,
                                             std::time(nullptr),
                                             lastBalance,
                                             refUserData,
                                             rules,
                                             duration,
                                             ecode);
}

bool Controller::storeErrorBlockingTime(const CardData &refUserData,
                                        const TransactionRules &rules,
                                        Duration &duration)
{
    ErrorCode::Code ecode = ErrorCode::Code::MANDIRI_D4_TAP_BELOW_ONE_MINUTE;
    switch (this->epayment.getType())
    {
    case Card::CARD_TYPE_BRI:
        ecode = ErrorCode::Code::BRI_A2_TAP_BELOW_ONE_MINUTE;
        break;
    case Card::CARD_TYPE_BNI:
        ecode = ErrorCode::Code::BNI_B9_TAP_BELOW_ONE_MINUTE;
        break;
    case Card::CARD_TYPE_BCA:
        ecode = ErrorCode::Code::BCA_E4_TAP_BELOW_ONE_MINUTE;
        break;
    case Card::CARD_TYPE_DKI:
        ecode = ErrorCode::Code::DKI_C9_TAP_BELOW_ONE_MINUTE;
        break;
    }
    this->storeErrorTransactionOnReadSuccess(true,
                                             false,
                                             std::time(nullptr),
                                             0,
                                             refUserData,
                                             rules,
                                             duration,
                                             ecode);
}

bool Controller::storeErrorFreeServiceExpired(const CardData &refUserData,
                                              const TransactionRules &rules,
                                              Duration &duration)
{
    this->storeErrorTransactionOnReadSuccess(true,
                                             false,
                                             std::time(nullptr),
                                             0,
                                             refUserData,
                                             rules,
                                             duration,
                                             ErrorCode::Code::DKI_C5_KLG_EXPIRED);
}

void Controller::routine()
{
    bool cardAvailable = false;
    bool result = false;

    { /* needed for duration calculation */
        Duration duration("transaction");
        cardAvailable = this->epayment.selectAttachedCard();
        if (cardAvailable)
        {
            duration.checkPoint("card pooling");
            try
            {
                result = this->processAttachedCard(duration);
            }
            catch (const std::exception &e)
            {
                Debug::info(__FILE__, __LINE__, __func__, "catch: %s\n", e.what());
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    if (cardAvailable)
    {
        if (result)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
        Debug::moveLogHistoryToFile();
        const SingleTripFare &singleTripFare = this->workflow.getProvision().getData().getPriceInformation().getSingleTrip();
        UIHelper::reset(this->gui, singleTripFare.getPrice());
    }
}

void Controller::reloadCounter()
{
    std::string counterPath = Counter::determineConfigPath(COUNTER_DATA_DIRECTORY, std::time(nullptr));
    this->counter.reset(new Counter(COUNTER_DATA_DIRECTORY, counterPath));
}

Controller::Controller(Epayment &epayment, WorkflowManager &workflow, Gui &gui) : isRun(false),
                                                                                  epayment(epayment),
                                                                                  workflow(workflow),
                                                                                  gui(gui),
                                                                                  th(),
                                                                                  mtx()
{
    this->reloadCounter();
}

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

                const SingleTripFare &singleTripFare = this->workflow.getProvision().getData().getPriceInformation().getSingleTrip();
                UIHelper::reset(this->gui, singleTripFare.getPrice());
                if (this->counter.get())
                    UIHelper::updateCounter(this->gui, this->counter.get());
                else
                    Debug::warning(__FILE__, __LINE__, __func__, "missing counter\n");
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