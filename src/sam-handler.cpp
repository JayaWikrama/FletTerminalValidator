#include "sam-handler.hpp"

#include "epayment/include/epayment.hpp"
#include "workflow/include/provision/payment-acceptance.hpp"
#include "communication/include/asa.hpp"
#include "gui/include/gui.hpp"
#include "utils/include/time.hpp"

SAMHandler::SAMStatus::SAMStatus() : active(false),
                                     initStatus(false),
                                     errorCode("") {}

SAMHandler::SAMStatus::~SAMStatus() {};

void SAMHandler::SAMStatus::setIsActiveStatus(bool isActive)
{
    std::lock_guard<std::mutex> guard(mtx);
    active = isActive;
}

void SAMHandler::SAMStatus::setInitSuccessStatus(bool isInitSuccess)
{
    std::lock_guard<std::mutex> guard(mtx);
    initStatus = isInitSuccess;
}

void SAMHandler::SAMStatus::setErrorCode(const std::string &error)
{
    std::lock_guard<std::mutex> guard(mtx);
    errorCode = error;
}

bool SAMHandler::SAMStatus::isActive() const
{
    std::lock_guard<std::mutex> guard(mtx);
    return active;
}

bool SAMHandler::SAMStatus::isInitSuccess() const
{
    std::lock_guard<std::mutex> guard(mtx);
    return initStatus;
}

const std::string &SAMHandler::SAMStatus::getErrorCode() const
{
    std::lock_guard<std::mutex> guard(mtx);
    return errorCode;
}

std::string SAMHandler::generateInitStatusMessage(const std::string &bank, const SAMStatus &samStatus, bool isPending, bool isProcess) const
{
    std::string message = "Initialize SAM " + bank;
    if (samStatus.isActive() == false)
    {
        message += " N/A";
    }
    else if (isPending)
    {
        message += "  - ";
    }
    else if (isProcess)
    {
        message += " ...";
    }
    else if (samStatus.isInitSuccess())
    {
        message += "  OK";
    }
    else
    {
        message += " ERR";
    }
    return message;
}

SAMHandler::SAMHandler(Epayment &epaymentRef) : mandiri(),
                                                bri(),
                                                bni(),
                                                bca(),
                                                dki(),
                                                epayment(epaymentRef) {}

SAMHandler::~SAMHandler() {};

bool SAMHandler::setupSAM(const PaymentAcceptance &p)
{
    if (p.getEmoney().getSlot() > 0)
    {
        if (epayment.setMandiriSamConfig(
                p.getEmoney().getSlot(),
                p.getEmoney().getPIN().c_str(),
                p.getEmoney().getIID().c_str(),
                p.getEmoney().getMID().c_str(),
                p.getEmoney().getTID().c_str()) == false)
        {
            return false;
        }
        this->mandiri.setIsActiveStatus(true);
    }
    else
    {
        this->mandiri.setIsActiveStatus(false);
    }
    if (p.getTapcash().getSlot() > 0)
    {
        if (epayment.setBNISamConfig(
                p.getTapcash()
                    .getSlot(),
                p.getTapcash().getMID().c_str(),
                p.getTapcash().getTID().c_str(),
                p.getTapcash().getMC().c_str()) == false)
        {
            return false;
        }
        this->bni.setIsActiveStatus(true);
    }
    else
    {
        this->bni.setIsActiveStatus(false);
    }
    if (p.getBrizzi().getSlot() > 0)
    {
        if (epayment.setBRISamConfig(
                p.getBrizzi().getSlot(),
                p.getBrizzi().getMID().c_str(),
                p.getBrizzi().getTID().c_str(),
                p.getBrizzi().getProcode().c_str(),
                1) == false)
        {
            return false;
        }
        this->bri.setIsActiveStatus(true);
    }
    else
    {
        this->bri.setIsActiveStatus(false);
    }
    if (p.getFlazz().getSlot() > 0)
    {
        if (epayment.setBCASamConfig(
                p.getFlazz().getSlot(),
                (p.getFlazz().getMID().length() == 15 ? p.getFlazz().getMID().c_str() + 3 : p.getFlazz().getMID().c_str()),
                p.getFlazz().getTID().c_str()) == false)
        {
            return false;
        }
        this->bca.setIsActiveStatus(true);
    }
    else
    {
        this->bca.setIsActiveStatus(false);
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
            return false;
        }
        this->dki.setIsActiveStatus(true);
    }
    else
    {
        this->dki.setIsActiveStatus(false);
    }
    return true;
}

bool SAMHandler::initSAM(Gui &ui)
{
    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, true),
         this->generateInitStatusMessage("BNI", this->bni, true, false),
         this->generateInitStatusMessage("BRI", this->bri, true, false),
         this->generateInitStatusMessage("BCA", this->bca, true, false),
         this->generateInitStatusMessage("DKI", this->dki, true, false)});

    if (this->mandiri.isActive())
    {
        this->mandiri.setInitSuccessStatus(this->epayment.initMandiriSAM(230400));
        if (this->mandiri.isInitSuccess() == false)
            this->mandiri.setErrorCode("SAME");
    }

    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, false),
         this->generateInitStatusMessage("BNI", this->bni, false, true),
         this->generateInitStatusMessage("BRI", this->bri, true, false),
         this->generateInitStatusMessage("BCA", this->bca, true, false),
         this->generateInitStatusMessage("DKI", this->dki, true, false)});

    if (this->bni.isActive())
    {
        this->bni.setInitSuccessStatus(this->epayment.initBNISAM(115200));
        if (this->bni.isInitSuccess() == false)
            this->bni.setErrorCode("SAME");
    }

    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, false),
         this->generateInitStatusMessage("BNI", this->bni, false, false),
         this->generateInitStatusMessage("BRI", this->bri, false, true),
         this->generateInitStatusMessage("BCA", this->bca, true, false),
         this->generateInitStatusMessage("DKI", this->dki, true, false)});

    if (this->bri.isActive())
    {
        this->bri.setInitSuccessStatus(this->epayment.initBRISAM(115200));
        if (this->bri.isInitSuccess() == false)
            this->bri.setErrorCode("SAME");
    }

    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, false),
         this->generateInitStatusMessage("BNI", this->bni, false, false),
         this->generateInitStatusMessage("BRI", this->bri, false, false),
         this->generateInitStatusMessage("BCA", this->bca, false, true),
         this->generateInitStatusMessage("DKI", this->dki, true, false)});

    if (this->bca.isActive())
    {
        this->bca.setInitSuccessStatus(this->epayment.initBCASAM(115200));
        if (this->bca.isInitSuccess() == false)
            this->bca.setErrorCode("SAME");
    }

    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, false),
         this->generateInitStatusMessage("BNI", this->bni, false, false),
         this->generateInitStatusMessage("BRI", this->bri, false, false),
         this->generateInitStatusMessage("BCA", this->bca, false, false),
         this->generateInitStatusMessage("DKI", this->dki, false, true)});

    if (this->dki.isActive())
    {
        this->dki.setInitSuccessStatus(this->epayment.initDKISAM(115200));
        if (this->dki.isInitSuccess() == false)
            this->dki.setErrorCode("SAME");
    }

    ui.message.show(
        {this->generateInitStatusMessage("MDR", this->mandiri, false, false),
         this->generateInitStatusMessage("BNI", this->bni, false, false),
         this->generateInitStatusMessage("BRI", this->bri, false, false),
         this->generateInitStatusMessage("BCA", this->bca, false, false),
         this->generateInitStatusMessage("DKI", this->dki, false, false)});
}

void SAMHandler::updateChannelModuleInitResult(ASAHeartBeatData &hb, const PaymentChannel &pc, const SAMStatus &s)
{
    if (s.isActive())
    {
        ASAPaymentChannel ch;
        ch.setId(pc.getId());
        ch.setStatus(s.isInitSuccess() ? 1 : 0);
        ch.setApiVersion("");
        ch.setCode(pc.getCode());
        ch.setLibVersion(this->epayment.getVersion());
        ch.setModuleError(s.getErrorCode());
        ch.setName(pc.getName());
        hb.addChannelModule(ch);
    }
}

void SAMHandler::updateChannelsModuleInitResult(ASA &asa, const PaymentAcceptance &p)
{
    std::string version = this->epayment.getVersion();
    asa.accessHeartBeatData(
        [this, &asa, &p, &version](ASAHeartBeatData &hb)
        {
            hb.getChannelModuleInitResult().clear();
            this->updateChannelModuleInitResult(hb, p.getEmoney(), this->getMandiriSAMStatus());
            this->updateChannelModuleInitResult(hb, p.getBrizzi(), this->getBriSAMStatus());
            this->updateChannelModuleInitResult(hb, p.getTapcash(), this->getBniSAMStatus());
            this->updateChannelModuleInitResult(hb, p.getFlazz(), this->getBcaSAMStatus());
            this->updateChannelModuleInitResult(hb, p.getJakcard(), this->getDkiSAMStatus());
        });
}

const SAMHandler::SAMStatus &SAMHandler::getMandiriSAMStatus() const
{
    return mandiri;
}

const SAMHandler::SAMStatus &SAMHandler::getBriSAMStatus() const
{
    return bri;
}

const SAMHandler::SAMStatus &SAMHandler::getBniSAMStatus() const
{
    return bni;
}

const SAMHandler::SAMStatus &SAMHandler::getBcaSAMStatus() const
{
    return bca;
}

const SAMHandler::SAMStatus &SAMHandler::getDkiSAMStatus() const
{
    return dki;
}