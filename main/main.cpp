#include <iostream>
#include <cstdlib>
#include <mutex>
#include <functional>
#include <thread>
#include <algorithm>

#include "controller.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "gui/include/gui.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"
#include "communication/include/fetch-api.hpp"

#include "utils/include/debug.hpp"
#include "utils/include/time.hpp"

std::string toFletCode(const std::array<unsigned char, 9> &flet)
{
    std::array<char, 10> result{};
    std::size_t idx = 0;
    for (const unsigned char &c : flet)
    {
        if (c == 0x20 || c == 0x00)
            continue;
        result[idx++] = static_cast<char>(c);
    }
    return result.data();
}

std::string toTerminal(const std::array<unsigned char, 8> &tid)
{
    std::array<char, 9> result{};
    std::size_t idx = 0;
    for (const unsigned char &c : tid)
    {
        if (c == 0x00)
            continue;
        result[idx++] = static_cast<char>(c);
    }
    return result.data();
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (argc != 2)
        {
            Debug::error(__FILE__, __LINE__, __func__, "command: %s <url>\n", argv[0]);
            return 1;
        }
        FetchAPI fapi(argv[1], 5, 10);
        fapi.get()
            .onSuccess(
                [](const std::string &payload)
                {
                    Debug::info(__FILE__, __LINE__, __func__, "GET Method success\n");
                    std::cout << payload << std::endl;
                })
            .onTimeout(
                []()
                {
                    Debug::error(__FILE__, __LINE__, __func__, "request timeout\n");
                })
            .onError(
                [](FetchAPI::ReturnCode code, const std::string &err)
                {
                    Debug::error(__FILE__, __LINE__, __func__, "%s\n", err.c_str());
                });
        return 0;
    }

    Debug::setMaxLinesLogCache(1024);
    Debug::setupTXTLogFile(MAIN_APP_LOG_DIRECTORY, MAIN_APP_LOG_FILE, 20971520UL, 5, 5);

    Sqlite3Transaction tscdb(TRANSACTION_DATABASE);
    tscdb.createLog();

    Gui gui;
    Epayment epayment;
    WorkflowManager workflow;

    if (workflow.loadProvision(PROVISION_CONFIG_FILE) == false)
    {
        Debug::critical(__FILE__, __LINE__, __func__, "invalid provision data: %s\n", PROVISION_CONFIG_FILE);
        exit(0);
    }

    Controller controller(epayment, workflow, gui);

    Debug::info(__FILE__, __LINE__, __func__, "epayment library version: %s\n", epayment.getVersion().c_str());

    const PaymentAcceptance &p = workflow.getProvision().getData().getPaymentAcceptance();

    if (p.getEmoney().getSlot() > 0)
        if (epayment.setMandiriSamConfig(
                p.getEmoney().getSlot(),
                p.getEmoney().getPIN().c_str(),
                p.getEmoney().getIID().c_str(),
                p.getEmoney().getMID().c_str(),
                p.getEmoney().getTID().c_str()) == false)
        {
            return 1;
        }
    if (p.getTapcash().getSlot() > 0)
        if (epayment.setBNISamConfig(
                p.getTapcash()
                    .getSlot(),
                p.getTapcash().getMID().c_str(),
                p.getTapcash().getTID().c_str(),
                p.getTapcash().getMC().c_str()) == false)
        {
            return 1;
        }
    if (p.getBrizzi().getSlot() > 0)
        if (epayment.setBRISamConfig(
                p.getBrizzi().getSlot(),
                p.getBrizzi().getMID().c_str(),
                p.getBrizzi().getTID().c_str(),
                p.getBrizzi().getProcode().c_str(),
                1) == false)
        {
            return 1;
        }
    if (p.getFlazz().getSlot() > 0)
        if (epayment.setBCASamConfig(
                p.getFlazz().getSlot(),
                (p.getFlazz().getMID().length() == 15 ? p.getFlazz().getMID().c_str() + 3 : p.getFlazz().getMID().c_str()),
                p.getFlazz().getTID().c_str()) == false)
        {
            return 1;
        }
    if (p.getJakcard().getSlot() > 0)
    {
        std::tm tmnow{};
        char formatedTm[16]{};
        TimeUtils::fromEpoch(&tmnow, std::time(nullptr));
        snprintf(formatedTm,
                 sizeof(formatedTm) - 1,
                 "%04d%02d%02d%02d%02d%02d",
                 tmnow.tm_year + 1900,
                 tmnow.tm_mon + 1,
                 tmnow.tm_mday,
                 tmnow.tm_hour,
                 tmnow.tm_min,
                 tmnow.tm_sec);
        formatedTm[14] = 0x00;
        if (epayment.setDKISamConfig(
                p.getJakcard().getSlot(),
                p.getJakcard().getMID().c_str(),
                p.getJakcard().getTID().c_str(),
                formatedTm,
                "dki-stan.json",
                1) == false)
        {
            return 1;
        }
    }

    controller.begin(
        [](Epayment &ep, WorkflowManager &workflow, Gui &ui)
        {
            bool samMandiri = false;
            bool samBni = false;
            bool samBri = false;
            bool samBca = false;
            bool samDki = false;

            ui.labelFletCode.setText(toFletCode(workflow.getIdentity().getFletCode()));
            ui.labelTerminalId.setText(toTerminal(workflow.getIdentity().getTerminalId()));

            ui.labelTariff.hide();
            ui.labelVersion.setText(ep.getVersion());

            ui.message.show(
                {"Initialize SAM MDR  ...",
                 "Initialize SAM BNI   - ",
                 "Initialize SAM BRI   - ",
                 "Initialize SAM BCA   - ",
                 "Initialize SAM DKI   - "});

            samMandiri = ep.initMandiriSAM(230400);

            ui.message.show(
                {"Initialize SAM MDR  " + std::string(samMandiri ? " OK" : "ERR"),
                 "Initialize SAM BNI  ...",
                 "Initialize SAM BRI   - ",
                 "Initialize SAM BCA   - ",
                 "Initialize SAM DKI   - "});

            samBni = ep.initBNISAM(115200);

            ui.message.show(
                {"Initialize SAM MDR  " + std::string(samMandiri ? " OK" : "ERR"),
                 "Initialize SAM BNI  " + std::string(samBni ? " OK" : "ERR"),
                 "Initialize SAM BRI  ...",
                 "Initialize SAM BCA   - ",
                 "Initialize SAM DKI   - "});

            samBri = ep.initBRISAM(115200);

            ui.message.show(
                {"Initialize SAM MDR  " + std::string(samMandiri ? " OK" : "ERR"),
                 "Initialize SAM BNI  " + std::string(samBni ? " OK" : "ERR"),
                 "Initialize SAM BRI  " + std::string(samBri ? " OK" : "ERR"),
                 "Initialize SAM BCA  ...",
                 "Initialize SAM DKI   - "});

            samBca = ep.initBCASAM(115200);

            ui.message.show(
                {"Initialize SAM MDR  " + std::string(samMandiri ? " OK" : "ERR"),
                 "Initialize SAM BNI  " + std::string(samBni ? " OK" : "ERR"),
                 "Initialize SAM BRI  " + std::string(samBri ? " OK" : "ERR"),
                 "Initialize SAM BCA  " + std::string(samBca ? " OK" : "ERR"),
                 "Initialize SAM DKI  ..."});

            samDki = ep.initDKISAM(115200);

            ui.message.show(
                {"Initialize SAM MDR  " + std::string(samMandiri ? " OK" : "ERR"),
                 "Initialize SAM BNI  " + std::string(samBni ? " OK" : "ERR"),
                 "Initialize SAM BRI  " + std::string(samBri ? " OK" : "ERR"),
                 "Initialize SAM BCA  " + std::string(samBca ? " OK" : "ERR"),
                 "Initialize SAM DKI  " + std::string(samDki ? " OK" : "ERR")});

            std::this_thread::sleep_for(std::chrono::seconds(1));

            ui.labelTariff.setRupiah(1, "Tarif", true);
            ui.labelStatus.hide();
            ui.message.hide();

            Debug::moveLogHistoryToFile();
        });

    gui.begin(argc, argv);
}
