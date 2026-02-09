#ifndef __UNCOMPLETE_WRITE_HANDLER_HPP__
#define __UNCOMPLETE_WRITE_HANDLER_HPP__

#include <ctime>
#include <string>
#include <deque>
#include <functional>

class UncompleteWriteData
{
private:
    bool used;
    unsigned int amount;
    unsigned int lastBalance;
    std::time_t time;
    unsigned long long pan;
    std::string mid;
    std::string tid;
    std::string issuer;
    std::string bank;
    std::string transcode;

public:
    UncompleteWriteData(const unsigned int amount,
                        const unsigned int lastBalance,
                        const std::time_t time,
                        const unsigned long long pan,
                        const std::string &mid,
                        const std::string &tid,
                        const std::string &issuer,
                        const std::string &bank,
                        const std::string &transcode);

    ~UncompleteWriteData();

    void setUsed();

    bool isUsed() const;
    unsigned int getAmount() const;
    unsigned int getLastBalance() const;
    std::time_t getTime() const;
    unsigned long long getPAN() const;
    const std::string &getMID() const;
    const std::string &getTID() const;
    const std::string &getIssuer() const;
    const std::string &getBank() const;
    const std::string &getTranscode() const;
};

class UncompleteWriteHandler
{
private:
    std::time_t timeKeeping;
    std::deque<UncompleteWriteData> data;

public:
    UncompleteWriteHandler();
    UncompleteWriteHandler(const std::time_t timeKeeping);
    ~UncompleteWriteHandler();

    void insert(const unsigned int amount,
                const unsigned int lastBalance,
                const std::time_t time,
                const unsigned long long pan,
                const std::string &mid,
                const std::string &tid,
                const std::string &issuer,
                const std::string &bank,
                const std::string &transcode);

    bool contain(const unsigned long long pan);
    bool contain(const unsigned long long pan, std::function<void(const UncompleteWriteData &)> handler);
    const UncompleteWriteData &getData(const unsigned long long pan);
    void setUsed(const unsigned long long pan);

    void maintain();
};

#endif