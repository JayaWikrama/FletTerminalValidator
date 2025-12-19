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

#include "communication/include/fetch-api.hpp"

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
            std::cout << "cmd: " << argv[0] << " <url>" << std::endl;
            return 1;
        }
        FetchAPI fapi(argv[1], 5, 10);
        fapi.get()
            .onSuccess(
                [](const std::string &payload)
                {
                    std::cout << "success to access URL" << std::endl;
                    std::cout << payload << std::endl;
                })
            .onTimeout(
                []()
                {
                    std::cout << "Request Timeout" << std::endl;
                })
            .onError(
                [](FetchAPI::ReturnCode code, const std::string &err)
                {
                    std::cout << "err: " << err << std::endl;
                });
        return 0;
    }

    Gui gui;
    Epayment epayment;
    WorkflowManager workflow;

    workflow.setType(WorkflowManager::Workflow::DEDUCT_ON_TAP_OUT);
    workflow.setIdentity(
        {0x20, 0x41, 0x31, 0x32, 0x33, 0x34, 0x43, 0x43, 0x41},
        {0x34, 0x31, 0x32, 0x30, 0x30, 0x31},
        CardData::Transportation::MICRO_TRANS);

    Controller controller(epayment, workflow, gui);

    std::cout << "Epayment library version " << epayment.getVersion() << std::endl;
    if (epayment.setMandiriSamConfig(1, "04A1F155E72EF8A2", "0019", "123456789012345", "32050100") == false)
    {
        return 1;
    }
    if (epayment.setBNISamConfig(1, "110990000123456", "00020103") == false)
    {
        return 1;
    }
    if (epayment.setBRISamConfig(1, "110990000123456", "00020103", "000012", 1) == false)
    {
        return 1;
    }
    if (epayment.setBCASamConfig(1, "885000123456", "EPP20103") == false)
    {
        return 1;
    }
    if (epayment.setDKISamConfig(3, "000900211000006", "00003516", "20241204130301", "dki-stan.json", 1) == false)
    {
        return 1;
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
        });

    gui.begin(argc, argv);
}
