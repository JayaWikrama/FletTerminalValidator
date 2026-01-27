#ifndef __COUNTER_HPP__
#define __COUNTER_HPP__

#include <string>
#include <mutex>
#include "utils/include/nlohmann/json_fwd.hpp"

class Counter
{
public:
    class Issuer
    {
    private:
        unsigned int tapInRegular;
        unsigned int tapInEconomy;
        unsigned int tapInFreeService;
        unsigned int tapOut;
        unsigned int sent;
        unsigned int pending;
        unsigned long long int amount;
        std::string filePath;
        mutable std::mutex mutex;

    public:
        Issuer(const std::string &filePath);
        ~Issuer();

        void incTapInRegular();
        void incTapInEconomy();
        void incTapInFreeService();
        void incTapOut();
        void incAmount(const unsigned int amount);
        void incPending();
        void incSent();

        unsigned int getTapInRegular() const;
        unsigned int getTapInEconomy() const;
        unsigned int getTapinFreeService() const;
        unsigned int getTapOut() const;
        unsigned int getPending() const;
        unsigned int getSent() const;
        unsigned long long int getAmount() const;

        void load();
        bool store();
        bool reset();
    };

    class Cycle
    {
    private:
        unsigned char day;
        unsigned char month;
        unsigned short year;

    public:
        Cycle();
        ~Cycle();

        std::time_t getCycleTime() const;
        bool isSameCycle(const std::time_t time) const;
    };

private:
    unsigned int sn;
    Cycle cycle;
    Issuer emoney;
    Issuer brizzi;
    Issuer tapcash;
    Issuer flazz;
    Issuer jakcard;
    std::string filePath;
    mutable std::mutex mutex;

    template <typename T>
    static void readUnsignedSafe(const nlohmann::json &j, const char *key, T &target);

public:
    Counter(const std::string &snPath, const std::string &counterPath);
    ~Counter();

    void incSN();

    const Cycle &getCycle() const;
    Issuer &getEmoney();
    Issuer &getBrizzi();
    Issuer &getTapcash();
    Issuer &getFlazz();
    Issuer &getJakcard();
    Issuer &getIssuerByEpaymentCardType(const unsigned int ctype);

    unsigned int getSN() const;
    unsigned int getTotalTapInRegular() const;
    unsigned int getTotalTapInEconomy() const;
    unsigned int getTotalTapInFreeService() const;
    unsigned int getTotalTapOut() const;
    unsigned int getTotalPending() const;
    unsigned int getTotalSent() const;
    unsigned long long int getTotalAmount() const;

    void loadSN();
    bool storeSN();
    bool resetSN();

    static std::string determineConfigPath(const std::string &basePath, const std::time_t time);
};

#endif