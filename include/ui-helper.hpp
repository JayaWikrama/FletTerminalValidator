#ifndef __UI_HELPER__
#define __UI_HELPER__

#include <string>
#include <mutex>

class Gui;

class UIHelper
{
private:
    static bool isStateProcessing;
    static std::mutex mtx;

public:
    enum class TariffType : unsigned char
    {
        REGULER = 0x00,
        JAKLINGKO = 0x01,
        ECONOMICAL = 0x02,
        FREE = 0x03
    };

    static void reset(Gui &gui, unsigned int amount);
    static void processingCard(Gui &gui);

    static void successTapInWithDeduct(Gui &gui, unsigned int amount, unsigned int baseAmount, unsigned int balance, TariffType type, std::time_t exp = 0);
    static void successTapOutWithoutDeduct(Gui &gui, unsigned int balance, UIHelper::TariffType type, std::time_t exp = 0);

    static void successTapOutWithDeduct(Gui &gui, unsigned int amount, unsigned int baseAmount, unsigned int balance, TariffType type, std::time_t exp = 0);
    static void successTapInWithoutDeduct(Gui &gui, unsigned int balance, UIHelper::TariffType type, std::time_t exp = 0);

    static void failedToReadCard(Gui &gui, const std::string &err);
    static void failedToWriteCard(Gui &gui, const std::string &err);
    static void failedToDeductCard(Gui &gui, const std::string &err);
    static void insufficientBalance(Gui &gui, unsigned int balance);
    static void blockingTime(Gui &gui);
    static void freeServiceExpired(Gui &gui, std::time_t exp);
    static void fareNotFound(Gui &gui);
};

#endif