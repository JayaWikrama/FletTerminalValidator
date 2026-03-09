#ifndef __SAM_HANDLER_HPP__
#define __SAM_HANDLER_HPP__

#include <string>
#include <mutex>

class Epayment;
class PaymentAcceptance;
class PaymentChannel;
class Gui;
class ASA;
class ASAHeartBeatData;

class SAMHandler
{
public:
    class SAMStatus
    {
    private:
        bool active;
        bool initStatus;
        std::string errorCode;
        mutable std::mutex mtx;

    public:
        SAMStatus();
        ~SAMStatus();

        void setIsActiveStatus(bool isActive);
        void setInitSuccessStatus(bool isInitSuccess);
        void setErrorCode(const std::string &error);

        bool isActive() const;
        bool isInitSuccess() const;
        const std::string &getErrorCode() const;
    };

private:
    SAMStatus mandiri;
    SAMStatus bri;
    SAMStatus bni;
    SAMStatus bca;
    SAMStatus dki;
    Epayment &epayment;

    bool bufferMarriageCode(const std::string &filePath, const std::string &marriageCode);
    bool getMarriageCodeBuffer(const std::string &filePath, std::string &marriageCode);

    std::string generateInitStatusMessage(const std::string &bank, const SAMStatus &samStatus, bool isPending, bool isProcess) const;

public:
    SAMHandler(Epayment &epayment);
    ~SAMHandler();

    bool setupSAM(const PaymentAcceptance &p);
    bool initSAM(Gui &ui);
    void updateChannelModuleInitResult(ASAHeartBeatData &hb, const PaymentChannel &pc, const SAMStatus &s);
    void updateChannelsModuleInitResult(ASA &asa, const PaymentAcceptance &p);

    const SAMStatus &getMandiriSAMStatus() const;
    const SAMStatus &getBriSAMStatus() const;
    const SAMStatus &getBniSAMStatus() const;
    const SAMStatus &getBcaSAMStatus() const;
    const SAMStatus &getDkiSAMStatus() const;
};

#endif