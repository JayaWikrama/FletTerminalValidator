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
#include "controller.hpp"
#include "epayment/include/epayment.hpp"
#include "workflow/include/workflow-manager.hpp"
#include "gui/include/gui.hpp"
#include "tscdata/include/sqlite3-transaction.hpp"
#include "communication/include/fetch-api.hpp"
#include "communication/include/asa.hpp"

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
    Debug::setupTXTLogFile(MAIN_APP_LOG_DIRECTORY, MAIN_APP_LOG_FILE, 20971520UL, 5, 5);

    Sqlite3Transaction tscdb(TRANSACTION_DATABASE);
    tscdb.createLog();

    Gui gui;
    Epayment epayment;
    WorkflowManager workflow;

    Debug::info(__FILE__, __LINE__, __func__, "epayment library version: %s\n", epayment.getVersion().c_str());

    ASA asa(COMM_CONFIG_FILE);
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

    asa.accessHeartBeatData(
        [&workflow](ASAHeartBeatData &hb)
        {
            const ProvisionData &provisionData = workflow.getProvision().getData();
            const BusinessEntityProfile &businessEntityProfile = provisionData.getBusinessEntityProfile();
            hb.getBusinessEntity().setId(businessEntityProfile.getId());
            hb.getBusinessEntity().setName(businessEntityProfile.getName());

            hb.setDeviceCode(provisionData.getCode());
            hb.setDeviceId(provisionData.getDeviceId());

            const DeviceMode &deviceMode = provisionData.getDeviceMode();
            hb.getDeviceMode().setId(deviceMode.getId());
            hb.getDeviceMode().setName(deviceMode.getName());

            const DeviceModel &deviceModel = provisionData.getDeviceModel();
            hb.getDeviceModel().setId(deviceModel.getId());
            hb.getDeviceModel().setName(deviceModel.getName());

            const FleetInformation &fleetInformation = provisionData.getLocation().getFleetInformation();
            hb.setDeviceName(fleetInformation.getName());

            hb.setDeviceVersion(provisionData.getDeviceVersion());
            hb.setFailed(0);

            hb.getLocation().setFleetId(fleetInformation.getFleetId());
            hb.getLocation().setName(fleetInformation.getName());

            hb.setLocationType(provisionData.getLocationType());
            hb.setMd5(provisionData.getMd5());

            const SingleTripFare &singleTripfare = provisionData.getPriceInformation().getSingleTrip();
            hb.setNormalFare(singleTripfare.getPrice());

            hb.setPing(36);         // ask
            hb.setReceiveBytes(0);  // ask
            hb.setReductionFare(0); // ask
            hb.setSendBytes(0);     // ask

#ifdef FTV_MODULE_VERSION
            hb.setSoftwareVersion(FTV_MODULE_VERSION);
#endif
        });

    GsmHandler gsmHandler;
    GpsHandler gpsHandler(asa);
    SAMHandler samHandler(epayment);

    gsmHandler.begin();
    gpsHandler.begin();
    if (samHandler.setupSAM(workflow.getProvision().getData().getPaymentAcceptance()) == false)
    {
        Debug::error(__FILE__, __LINE__, __func__, "some SAM configuration error\n");
        return 1;
    }

    Controller controller(epayment, workflow, gpsHandler, gsmHandler, samHandler, asa, gui);

    TscDeliveryHandler tscDeliveryHandler(asa, gsmHandler, workflow, controller);
    tscDeliveryHandler.setTransactionLocalDatabase(TRANSACTION_DATABASE);
    tscDeliveryHandler.begin();

    controller.begin(
        [](SAMHandler &samHandler, WorkflowManager &workflow, ASA &asa, Gui &ui)
        {
            bool samMandiri = false;
            bool samBni = false;
            bool samBri = false;
            bool samBca = false;
            bool samDki = false;

            ui.labelFletCode.setText(toFleetCode(workflow.getIdentity().getFleetCode()));
            ui.labelTerminalId.setText(toTerminal(workflow.getIdentity().getTerminalId()));

            ui.labelTariff.hide();
#ifdef FTV_MODULE_VERSION
            ui.labelVersion.setText(FTV_MODULE_VERSION);
#endif

            samHandler.initSAM(ui);
            samHandler.updateChannelsModuleInitResult(asa, workflow.getProvision().getData().getPaymentAcceptance());

            std::this_thread::sleep_for(std::chrono::seconds(1));

            ui.labelTariff.setRupiah(1, "Tarif", true);
            ui.labelStatus.hide();
            ui.message.hide();

            Debug::moveLogHistoryToFile();
        });

    gui.begin(argc, argv);
}
