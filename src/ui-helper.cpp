#include <thread>
#include <algorithm>
#include "ui-helper.hpp"
#include "gui/include/gui.hpp"

bool UIHelper::isStateProcessing = false;
std::mutex UIHelper::mtx;

std::string formatRupiah(unsigned int value, const std::string &pre = "")
{
    std::string number = std::to_string(value);
    std::string formatted;

    int count = 0;
    for (int i = number.length() - 1; i >= 0; --i)
    {
        formatted.insert(formatted.begin(), number[i]);
        count++;

        if (count % 3 == 0 && i != 0)
        {
            formatted.insert(formatted.begin(), '.');
        }
    }
    if (pre.empty())
        return "RP " + formatted;
    return pre + " RP " + formatted;
}

std::string formatDate(std::time_t time, const std::string &pre = "")
{
    char dte[32]{};
    std::tm tmtmp{};

    localtime_r(&time, &tmtmp);
    snprintf(dte,
             sizeof(dte) - 1,
             "%02d-%02d-%02d",
             tmtmp.tm_mday,
             tmtmp.tm_mon + 1,
             (tmtmp.tm_year + 1900) % 100);

    if (pre.empty())
        return std::string(dte);
    return pre + " " + std::string(dte);
}

void UIHelper::reset(Gui &gui, unsigned int amount)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.labelTariff.setRupiah(amount, "Tarif");
    gui.labelStatus.hide();
    gui.labelCardNumber.hide();
    gui.labelBalance.hide();
    gui.message.hide();
}

void UIHelper::processingCard(Gui &gui)
{
    {
        std::lock_guard<std::mutex> guard(UIHelper::mtx);
        UIHelper::isStateProcessing = true;
    }
    std::thread(
        [&gui]()
        {
            std::string message = "Sedang diproses";
            gui.labelStatus.setText(message, true);
            for (;;)
            {
                {
                    std::lock_guard<std::mutex> guard(UIHelper::mtx);
                    if (UIHelper::isStateProcessing == false)
                    {
                        break;
                    }
                    gui.labelStatus.setText(message);
                    if (message.length() == 18)
                        message = "Sedang diproses";
                    else
                        message += ".";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
            }
        })
        .detach();
}

void UIHelper::successTapInWithDeduct(Gui &gui, unsigned int amount, unsigned int baseAmount, unsigned int balance, UIHelper::TariffType type, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    switch (type)
    {
    case UIHelper::TariffType::REGULER:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-IN SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::JAKLINGKO:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-IN SUKSES JAKLINGKO",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::FREE:
        gui.message.show(
            {"TAP-IN SUKSES",
             formatDate(exp, "BERLAKU s/d"),
             "LAYANAN GRATIS",
             "PEMPROV DKI JAKARTA",
             " "});
        break;
    default:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-IN SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    }
}

void UIHelper::successTapOutWithoutDeduct(Gui &gui, unsigned int balance, UIHelper::TariffType type, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    switch (type)
    {
    case UIHelper::TariffType::REGULER:
        gui.message.show(
            {"TAP-OUT SUKSES",
             " ",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    case UIHelper::TariffType::FREE:
        gui.message.show(
            {"TAP-OUT SUKSES",
             formatDate(exp, "BERLAKU s/d"),
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    case UIHelper::TariffType::JAKLINGKO:
        gui.message.show(
            {"TAP-OUT SUKSES",
             "JAKLINGKO",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    default:
        gui.message.show(
            {"TAP-OUT SUKSES",
             " ",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    }
}

void UIHelper::successTapOutWithDeduct(Gui &gui, unsigned int amount, unsigned int baseAmount, unsigned int balance, UIHelper::TariffType type, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    switch (type)
    {
    case UIHelper::TariffType::REGULER:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-OUT SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::JAKLINGKO:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-OUT SUKSES JAKLINGKO",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::FREE:
        gui.message.show(
            {"TAP-OUT SUKSES",
             formatDate(exp, "BERLAKU s/d"),
             "LAYANAN GRATIS",
             "PEMPROV DKI JAKARTA",
             " "});
        break;
    default:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "TAP-OUT SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    }
}

void UIHelper::successTapInWithoutDeduct(Gui &gui, unsigned int balance, UIHelper::TariffType type, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    switch (type)
    {
    case UIHelper::TariffType::REGULER:
        gui.message.show(
            {"TAP-IN SUKSES",
             " ",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    case UIHelper::TariffType::FREE:
        gui.message.show(
            {"TAP-IN SUKSES",
             formatDate(exp, "BERLAKU s/d"),
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    case UIHelper::TariffType::JAKLINGKO:
        gui.message.show(
            {"TAP-IN SUKSES",
             "JAKLINGKO",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    default:
        gui.message.show(
            {"TAP-IN SUKSES",
             " ",
             "TIDAK TERPOTONG",
             formatRupiah(balance, "SISA SALDO"),
             " "});
        break;
    }
}

void UIHelper::successResetTapIn(Gui &gui, unsigned int amount, unsigned int baseAmount, unsigned int balance, UIHelper::TariffType type, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    switch (type)
    {
    case UIHelper::TariffType::REGULER:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "RESET + TAP-IN SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::JAKLINGKO:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "RESET + TAP-IN SUKSES JAKLINGKO",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    case UIHelper::TariffType::FREE:
        gui.message.show(
            {"RESET + TAP-IN SUKSES",
             formatDate(exp, "BERLAKU s/d"),
             "LAYANAN GRATIS",
             "PEMPROV DKI JAKARTA",
             " "});
        break;
    default:
        gui.message.show(
            {formatRupiah(baseAmount, "TARIF REGULAR"),
             "RESET + TAP-IN SUKSES",
             " ",
             formatRupiah(amount, "TERPOTONG"),
             formatRupiah(balance, "SALDO ANDA")});
        break;
    }
}

void UIHelper::failedToReadCard(Gui &gui, const std::string &err)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"MASALAH",
         "PADA KARTU",
         " ",
         " ",
         err});
}

void UIHelper::failedToWriteCard(Gui &gui, const std::string &err)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"MASALAH",
         "PADA KARTU",
         " ",
         " ",
         err});
}

void UIHelper::failedToDeductCard(Gui &gui, const std::string &err)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"MASALAH",
         "PADA KARTU",
         " ",
         " ",
         err});
}

void UIHelper::insufficientBalance(Gui &gui, unsigned int balance)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"SALDO KURANG",
         "SILAHKAN ISI SALDO",
         " ",
         " ",
         "TERIMA KASIH"});
}

void UIHelper::blockingTime(Gui &gui)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"KARTU SUDAH DI",
         "GUNAKAN",
         "SILAHKAN TUNGGU",
         "ATAU NAIK BERIKUTNYA",
         ""});
}

void UIHelper::freeServiceExpired(Gui &gui, std::time_t exp)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"KARTU HABIS MASA",
         formatDate(exp, "BERLAKU s/d"),
         " ",
         "LAKUKAN PERPANJANGAN",
         ""});
}

void UIHelper::fareNotFound(Gui &gui)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"TARIF",
         " ",
         "TIDAK",
         "DITEMUKAN",
         ""});
}

void UIHelper::insufficientMinimumBalance(Gui &gui, unsigned int balance)
{
    std::lock_guard<std::mutex> guard(UIHelper::mtx);
    UIHelper::isStateProcessing = false;
    gui.message.show(
        {"SALDO MINIMUM KURANG",
         formatRupiah(balance, "SALDO ANDA"),
         " ",
         "SILAHKAN ISI SALDO",
         "TERIMA KASIH"});
}