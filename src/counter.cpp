#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include "counter.hpp"
#include "epayment/include/card-access.hpp"
#include "utils/include/nlohmann/json.hpp"
#include "utils/include/debug.hpp"
#include "utils/include/error.hpp"
#include "utils/include/time.hpp"

static bool createDirectory(const std::string &path)
{
    Debug::info(__FILE__, __LINE__, __func__, "create directory \"%s\"...\n", path.c_str());
    if (mkdir(path.c_str(), 0777) == 0)
    {
        Debug::info(__FILE__, __LINE__, __func__, "create directory \"%s\" success\n", path.c_str());
        return true;
    }
    else if (errno == EEXIST)
    {
        Debug::info(__FILE__, __LINE__, __func__, "directory \"%s\" already exist\n", path.c_str());
        return true;
    }
    Debug::error(__FILE__, __LINE__, __func__, "create directory \"%s\" failed: %s\n", path.c_str(), strerror(errno));
    return false;
}

Counter::Issuer::Issuer(const std::string &filePath) : tapInRegular(0U),
                                                       tapInEconomy(0U),
                                                       tapInFreeService(0U),
                                                       tapOut(0U),
                                                       sent(0U),
                                                       pending(0U),
                                                       amount(0ULL),
                                                       filePath(filePath),
                                                       mutex()
{
    this->load();
}

Counter::Issuer::~Issuer()
{
    this->store();
}

void Counter::Issuer::incTapInRegular()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->tapInRegular++;
}

void Counter::Issuer::incTapInEconomy()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->tapInEconomy++;
}

void Counter::Issuer::incTapInFreeService()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->tapInFreeService++;
}

void Counter::Issuer::incTapOut()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->tapOut++;
}

void Counter::Issuer::incAmount(const unsigned int amount)
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->amount += amount;
}

void Counter::Issuer::incPending()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->pending++;
}

void Counter::Issuer::incSent()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->sent++;
    this->pending--;
}

unsigned int Counter::Issuer::getTapInRegular() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->tapInRegular;
}

unsigned int Counter::Issuer::getTapInEconomy() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->tapInEconomy;
}

unsigned int Counter::Issuer::getTapinFreeService() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->tapInFreeService;
}

unsigned int Counter::Issuer::getTapOut() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->tapOut;
}

unsigned int Counter::Issuer::getPending() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->sent;
}

unsigned int Counter::Issuer::getSent() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->pending;
}

unsigned long long int Counter::Issuer::getAmount() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->amount;
}

void Counter::Issuer::load()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    Debug::info(__FILE__, __LINE__, __func__, "load %s configuartion\n", this->filePath.c_str());
    std::ifstream file(filePath);
    if (!file.is_open())
        /* file doesn't exist */
        return;

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, "parse \"" + filePath + "\" failed"));
    }

    if (!j.is_object())
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, "configuuration in \"" + filePath + "\" is not JSON object"));

    Counter::readUnsignedSafe(j, "tap_in_regular", tapInRegular);
    Counter::readUnsignedSafe(j, "tap_in_economy", tapInEconomy);
    Counter::readUnsignedSafe(j, "tap_in_free_service", tapInFreeService);
    Counter::readUnsignedSafe(j, "tap_out", tapOut);
    Counter::readUnsignedSafe(j, "sent", sent);
    Counter::readUnsignedSafe(j, "pending", pending);
    Counter::readUnsignedSafe(j, "amount", amount);

    Debug::info(__FILE__, __LINE__, __func__, "load %s configuartion done\n", this->filePath.c_str());
}

bool Counter::Issuer::store()
{
    std::lock_guard<std::mutex> guard(this->mutex);

    nlohmann::json j;

    j["tap_in_regular"] = tapInRegular;
    j["tap_in_economy"] = tapInEconomy;
    j["tap_in_free_service"] = tapInFreeService;
    j["tap_out"] = tapOut;
    j["sent"] = sent;
    j["pending"] = pending;
    j["amount"] = amount;

    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open())
        return false;

    try
    {
        file << j.dump(4);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool Counter::Issuer::reset()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->tapInRegular = 0U;
    this->tapInEconomy = 0U;
    this->tapInFreeService = 0U;
    this->tapOut = 0U;
    this->sent = 0U;
    this->pending = 0U;
    this->amount = 0ULL;
}

Counter::Cycle::Cycle() : day(0x00U),
                          month(0x00U),
                          year(0x0000U)
{
    std::tm tmp{};
    TimeUtils::fromEpoch(&tmp, std::time(nullptr));
    this->year = static_cast<unsigned short>(tmp.tm_year + 1900);
    this->month = static_cast<unsigned char>(tmp.tm_mon + 1);
    this->day = static_cast<unsigned char>(tmp.tm_mday);
}

Counter::Cycle::~Cycle() {}

std::time_t Counter::Cycle::getCycleTime() const
{
    std::tm tmp{};
    memset(&tmp, 0x00, sizeof(tmp));
    tmp.tm_year = static_cast<int>(this->year - 1900);
    tmp.tm_mon = static_cast<int>(this->month - 1);
    tmp.tm_mday = static_cast<int>(this->day);
    return TimeUtils::toEpoch(&tmp);
}

bool Counter::Cycle::isSameCycle(const std::time_t time) const
{
    std::tm tmp{};
    TimeUtils::fromEpoch(&tmp, std::time(nullptr));
    return (this->year == static_cast<unsigned short>(tmp.tm_year + 1900) ||
            this->month == static_cast<unsigned char>(tmp.tm_mon + 1) ||
            this->day == static_cast<unsigned char>(tmp.tm_mday));
}

template <typename T>
void Counter::readUnsignedSafe(const nlohmann::json &j, const char *key, T &target)
{
    if (!j.contains(key))
        throw std::runtime_error(Error::fieldNotFound(__FILE__, __LINE__, __func__, key));

    if (!j[key].is_number_unsigned())
        throw std::runtime_error(Error::fieldTypeInvalid(__FILE__, __LINE__, __func__, "unsigned number", key));

    unsigned long long value = j[key].get<unsigned long long>();

    if (value > std::numeric_limits<T>::max())
        throw std::runtime_error(Error::outOfRange(__FILE__, __LINE__, __func__, key));

    target = static_cast<T>(value);
}

template void Counter::readUnsignedSafe(const nlohmann::json &j, const char *key, unsigned int &target);
template void Counter::readUnsignedSafe(const nlohmann::json &j, const char *key, unsigned long long int &target);

Counter::Counter(const std::string &snPath, const std::string &counterPath) : sn(0),
                                                                              cycle(),
                                                                              emoney(counterPath + "/emoney.json"),
                                                                              brizzi(counterPath + "/brizzi.json"),
                                                                              tapcash(counterPath + "/tapcash.json"),
                                                                              flazz(counterPath + "/flazz.json"),
                                                                              jakcard(counterPath + "/jakcard.json"),
                                                                              filePath(snPath + "/sn.json"),
                                                                              mutex()
{
    this->loadSN();
}

Counter::~Counter()
{
    this->storeSN();
}

void Counter::incSN()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    this->sn++;
}

const Counter::Cycle &Counter::getCycle() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->cycle;
}

Counter::Issuer &Counter::getEmoney()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->emoney;
}

Counter::Issuer &Counter::getBrizzi()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->brizzi;
}

Counter::Issuer &Counter::getTapcash()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->tapcash;
}

Counter::Issuer &Counter::getFlazz()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->flazz;
}

Counter::Issuer &Counter::getJakcard()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    return this->jakcard;
}

Counter::Issuer &Counter::getIssuerByEpaymentCardType(const unsigned int ctype)
{
    std::lock_guard<std::mutex> guard(this->mutex);
    switch (static_cast<Card::cardType_t>(ctype))
    {
    case Card::CARD_TYPE_BRI:
        return this->brizzi;
        break;
    case Card::CARD_TYPE_BNI:
        return this->tapcash;
        break;
    case Card::CARD_TYPE_BCA:
        return this->flazz;
        break;
    case Card::CARD_TYPE_DKI:
        return this->jakcard;
        break;
    }
    return this->emoney;
}

unsigned int Counter::getSN() const
{
    return this->sn;
}

unsigned int Counter::getTotalTapInRegular() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getTapInRegular() +
                          this->brizzi.getTapInRegular() +
                          this->tapcash.getTapInRegular() +
                          this->flazz.getTapInRegular() +
                          this->jakcard.getTapInRegular();
    return result;
}

unsigned int Counter::getTotalTapInEconomy() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getTapInEconomy() +
                          this->brizzi.getTapInEconomy() +
                          this->tapcash.getTapInEconomy() +
                          this->flazz.getTapInEconomy() +
                          this->jakcard.getTapInEconomy();
    return result;
}

unsigned int Counter::getTotalTapInFreeService() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getTapinFreeService() +
                          this->brizzi.getTapinFreeService() +
                          this->tapcash.getTapinFreeService() +
                          this->flazz.getTapinFreeService() +
                          this->jakcard.getTapinFreeService();
    return result;
}

unsigned int Counter::getTotalTapOut() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getTapOut() +
                          this->brizzi.getTapOut() +
                          this->tapcash.getTapOut() +
                          this->flazz.getTapOut() +
                          this->jakcard.getTapOut();
    return result;
}

unsigned int Counter::getTotalPending() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getPending() +
                          this->brizzi.getPending() +
                          this->tapcash.getPending() +
                          this->flazz.getPending() +
                          this->jakcard.getPending();
    return result;
}

unsigned int Counter::getTotalSent() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned int result = this->emoney.getSent() +
                          this->brizzi.getSent() +
                          this->tapcash.getSent() +
                          this->flazz.getSent() +
                          this->jakcard.getSent();
    return result;
}

unsigned long long int Counter::getTotalAmount() const
{
    std::lock_guard<std::mutex> guard(this->mutex);
    unsigned long long int result = this->emoney.getAmount() +
                                    this->brizzi.getAmount() +
                                    this->tapcash.getAmount() +
                                    this->flazz.getAmount() +
                                    this->jakcard.getAmount();
    return result;
}

void Counter::loadSN()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    Debug::info(__FILE__, __LINE__, __func__, "load %s configuartion\n", this->filePath.c_str());
    std::ifstream file(filePath);
    if (!file.is_open())
        /* file doesn't exist */
        return;

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, "parse \"" + filePath + "\" failed"));
    }

    if (!j.is_object())
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, "configuuration in \"" + filePath + "\" is not JSON object"));

    Counter::readUnsignedSafe(j, "sn", this->sn);

    Debug::info(__FILE__, __LINE__, __func__, "load %s configuartion done\n", this->filePath.c_str());
}

bool Counter::storeSN()
{
    std::lock_guard<std::mutex> guard(this->mutex);

    nlohmann::json j;

    j["sn"] = this->sn;

    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open())
        return false;

    try
    {
        file << j.dump(4);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool Counter::resetSN()
{
    this->sn = 0U;
}

std::string Counter::determineConfigPath(const std::string &basePath, const std::time_t time)
{
    std::string result = basePath;

    std::tm tmp{};
    TimeUtils::fromEpoch(&tmp, time);

    result += "/" + TimeUtils::format(&tmp, TimeUtils::TIME_FORMAT_YEAR_ONLY);
    if (!createDirectory(result))
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, result + " directory creation failed"));

    result += "/" + TimeUtils::format(&tmp, TimeUtils::TIME_FORMAT_MONTH_SHORT);
    if (!createDirectory(result))
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, result + " directory creation failed"));

    result += "/" + TimeUtils::format(&tmp, TimeUtils::TIME_FORMAT_ISO_DATE);
    if (!createDirectory(result))
        throw std::runtime_error(Error::common(__FILE__, __LINE__, __func__, result + " directory creation failed"));

    return result;
}