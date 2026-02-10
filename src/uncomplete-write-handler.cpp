#include <stdexcept>
#include "uncomplete-write-handler.hpp"
#include "utils/include/error.hpp"
#include "utils/include/debug.hpp"

UncompleteWriteData::UncompleteWriteData(
    const unsigned int amount,
    const unsigned int lastBalance,
    const std::time_t time,
    const unsigned long long pan,
    const std::string &mid,
    const std::string &tid,
    const std::string &issuer,
    const std::string &bank,
    const std::string &transcode)
    : used(false),
      amount(amount),
      lastBalance(lastBalance),
      time(time),
      pan(pan),
      mid(mid),
      tid(tid),
      issuer(issuer),
      bank(bank),
      transcode(transcode) {}

UncompleteWriteData::~UncompleteWriteData() {}

void UncompleteWriteData::setUsed()
{
    this->used = true;
}

bool UncompleteWriteData::isUsed() const
{
    return this->used;
}

unsigned int UncompleteWriteData::getAmount() const
{
    return this->amount;
}

unsigned int UncompleteWriteData::getLastBalance() const
{
    return this->lastBalance;
}

std::time_t UncompleteWriteData::getTime() const
{
    return this->time;
}

unsigned long long UncompleteWriteData::getPAN() const
{
    return this->pan;
}

const std::string &UncompleteWriteData::getMID() const
{
    return this->mid;
}

const std::string &UncompleteWriteData::getTID() const
{
    return this->tid;
}

const std::string &UncompleteWriteData::getIssuer() const
{
    return this->issuer;
}

const std::string &UncompleteWriteData::getBank() const
{
    return this->bank;
}

const std::string &UncompleteWriteData::getTranscode() const
{
    return this->transcode;
}

UncompleteWriteHandler::UncompleteWriteHandler() : timeKeeping(10),
                                                   data() {}

UncompleteWriteHandler::UncompleteWriteHandler(const std::time_t timeKeeping) : timeKeeping(timeKeeping),
                                                                                data() {}

UncompleteWriteHandler::~UncompleteWriteHandler() {}

void UncompleteWriteHandler::insert(const unsigned int amount,
                                    const unsigned int lastBalance,
                                    const std::time_t time,
                                    const unsigned long long pan,
                                    const std::string &mid,
                                    const std::string &tid,
                                    const std::string &issuer,
                                    const std::string &bank,
                                    const std::string &transcode)
{
    this->data.emplace_back(amount, lastBalance, time, pan, mid, tid, issuer, bank, transcode);
}

bool UncompleteWriteHandler::contain(const unsigned long long pan)
{
    std::time_t tcurrent = std::time(nullptr);
    for (const UncompleteWriteData &d : this->data)
    {
        if (d.getPAN() == pan && (tcurrent - d.getTime() <= this->timeKeeping))
            return true;
    }
    return false;
}

bool UncompleteWriteHandler::contain(const unsigned long long pan, std::function<void(const UncompleteWriteData &)> handler)
{
    std::time_t tcurrent = std::time(nullptr);
    for (const UncompleteWriteData &d : this->data)
    {
        if (d.getPAN() == pan && (tcurrent - d.getTime() <= this->timeKeeping))
        {
            Debug::info(__FILE__, __LINE__, __func__, "uncomplete write handler for: %llu...\n", pan);
            handler(d);
            return true;
        }
    }
    return false;
}

const UncompleteWriteData &UncompleteWriteHandler::getData(const unsigned long long pan)
{
    for (const UncompleteWriteData &d : this->data)
    {
        if (d.getPAN() == pan)
            return d;
    }
    throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, "data not found"));
}

void UncompleteWriteHandler::setUsed(const unsigned long long pan)
{
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it->getPAN() == pan)
        {
            data.erase(it);
            break;
        }
    }
}

void UncompleteWriteHandler::maintain()
{
    std::time_t tcurrent = std::time(nullptr);
    for (auto it = data.begin(); it != data.end();)
    {
        if ((tcurrent - it->getTime()) > this->timeKeeping || it->isUsed())
        {
            it = data.erase(it);
        }
        else
        {
            ++it;
        }
    }
}