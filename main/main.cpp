#include <iostream>
#include <cstdlib>
#include <mutex>
#include <functional>
#include <thread>
#include <algorithm>
#include <cstring>

#include "gsm-handler.hpp"
#include "gps-handler.hpp"
#include "sam-handler.hpp"
#include "tsc-delivery-handler.hpp"
#include "setup.hpp"
#include "controller.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "gui/include/gui.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"
#include "communication/include/fetch-api.hpp"
#include "communication/include/asa.hpp"
#include "communication/include/tjs.hpp"

#include "utils/include/debug.hpp"
#include "utils/include/time.hpp"

std::string toFleetCode(const std::array<unsigned char, 9> &fleet)
{
    std::array<char, 10> result{};
    std::size_t idx = 0;
    for (const unsigned char &c : fleet)
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
    bool isWithProvisioning = true;
    if (argc > 1)
    {
        if (strcmp(argv[1], "--without-provisioning") == 0)
        {
            isWithProvisioning = false;
        }
        else
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
    }

    Debug::setMaxLinesLogCache(1024);
    Debug::setupTXTLogFile(MAIN_APP_LOG_DIRECTORY, MAIN_APP_LOG_FILE, 10485760UL, 5, 5);

    Sqlite3Transaction localTscDatabase(TRANSACTION_DATABASE);
    localTscDatabase.createLog();

    Gui gui;
    Epayment epayment;
    WorkflowManager workflow;

    Debug::info(__FILE__, __LINE__, __func__, "epayment library version: %s\n", epayment.getVersion().c_str());

    ASA asa(COMM_CONFIG_FILE);
    TJS tjs(CTJS_CONFIG_FILE);

    GsmHandler gsmHandler;
    GpsHandler gpsHandler(asa);
    SAMHandler samHandler(epayment);

    gsmHandler.begin();
    gpsHandler.begin();

    Controller controller(epayment, workflow, gpsHandler, gsmHandler, samHandler, asa, localTscDatabase, gui);
    TscDeliveryHandler tscDeliveryHandler(asa, tjs, gsmHandler, workflow, controller, localTscDatabase, gui);
    tscDeliveryHandler.begin();

    controller.begin(
        [&isWithProvisioning, &tjs](SAMHandler &samHandler, WorkflowManager &workflow, ASA &asa, Gui &ui)
        {
            /* Preparation */
            ui.setWindowBackground(true);
            ui.message.show({"",
                             "LOAD",
                             "CONFIGURATION",
                             "",
                             ""});

            while (asa.load() == false)
            {
                ui.setUnderMaintenance(true);
                Setup communicationSetup;
                if (communicationSetup.loadIMEI(IMEI_PNG) == false)
                {
                    Debug::error(__FILE__, __LINE__, __func__, "failed to load imei\n");
                }

                while (communicationSetup.setup(COMM_CONFIG_FILE) == false)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            /* process ASA Login & Provision */
            ui.message.show({"",
                             "DEVICE",
                             "PROVISIONING",
                             "",
                             ""});
            asa.login();
            if (isWithProvisioning)
                asa.provision(PROVISION_CONFIG_FILE);
            else
                Debug::warning(__FILE__, __LINE__, __func__, "skip download provision.json\n");

            if (workflow.loadProvision(PROVISION_CONFIG_FILE) == false)
            {
                Debug::critical(__FILE__, __LINE__, __func__, "invalid provision data: %s\n", PROVISION_CONFIG_FILE);
                exit(0);
            }

            const ProvisionData &provisionData = workflow.getProvision().getData();

            workflow.accessIdentity(
                [&provisionData](TerminalIdentity &terminalIdentity)
                {
                    terminalIdentity.setTerminalCode(provisionData.getTransJakartaConfig().getTerminalCode());
                });

            /* Process TJ Login */
            tjs.setBaseUrl(provisionData.getEndpoint().getApitoLoginThirdParty());
            tjs.setUserName(provisionData.getTransJakartaConfig().getTerminalCode());
            tjs.setKey(provisionData.getTransJakartaConfig().getPassword());
            tjs.login();

            /* Process SAM */
            if (samHandler.setupSAM(workflow.getProvision().getData().getPaymentAcceptance()) == false)
            {
                Debug::error(__FILE__, __LINE__, __func__, "some SAM configuration error\n");
                return 1;
            }

            bool samMandiri = false;
            bool samBni = false;
            bool samBri = false;
            bool samBca = false;
            bool samDki = false;

            ui.labelFleetCode.setText(toFleetCode(workflow.getIdentity().getFleetCode()));
            ui.labelTerminalId.setText(toTerminal(workflow.getIdentity().getTerminalId()));
            ui.labelTerminalName.setText(workflow.getProvision().getData().getLocation().getFleetInformation().getTerminalName());

            ui.labelTariff.hide();
#ifdef FTV_MODULE_VERSION
            std::string version = FTV_MODULE_VERSION;
            ui.labelVersion.setText(("V: " + version.substr(0, version.find('-'))).c_str());
#endif

            samHandler.initSAM(ui);
            samHandler.updateChannelsModuleInitResult(asa, workflow.getProvision().getData().getPaymentAcceptance());

            std::this_thread::sleep_for(std::chrono::seconds(1));

            ui.labelTariff.setRupiah(1, "Tarif", true);
            ui.labelStatus.hide();
            ui.message.hide();

            Debug::info(__FILE__, __LINE__, __func__, "store startup log\n");
            Debug::moveLogHistoryToFile();

            Debug::info(__FILE__, __LINE__, __func__, "trigger tsc delivery signal\n");
            TscDeliveryHandler::signal();
            Debug::info(__FILE__, __LINE__, __func__, "preparation done\n");
        });

    gui.begin(argc, argv);
}
