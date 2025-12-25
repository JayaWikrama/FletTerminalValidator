#include "duration.hpp"
#include "utils/include/debug.hpp"

std::chrono::steady_clock::time_point timePoint;
std::string caption;

Duration::PointRefs::PointRefs(const std::string &caption) : timePoint(std::chrono::steady_clock::now()), caption(caption) {}

Duration::PointRefs::~PointRefs() {}

const std::chrono::steady_clock::time_point &Duration::PointRefs::getTime() const
{
    return this->timePoint;
}

const std::string &Duration::PointRefs::getCaption() const
{
    return this->caption;
}

double Duration::PointRefs::diff(const std::chrono::steady_clock::time_point &ref) const
{
    std::chrono::duration<double> diff = this->timePoint - ref;
    return diff.count();
}

void Duration::printDiffTime(const PointRefs &pref, const std::chrono::steady_clock::time_point &sref) const
{
    Debug::info(__FILE__, __LINE__, "elapsed time", "%s: %.03f\n", pref.getCaption().c_str(), pref.diff(sref));
}

Duration::Duration(const std::string &caption) : startRef(std::chrono::steady_clock::now()), pointRefs(), caption(caption)
{
    this->pointRefs.reserve(8);
}

Duration::~Duration()
{
    std::chrono::steady_clock::time_point endRef = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = endRef - this->startRef;
    std::size_t sz = this->pointRefs.size();
    if (sz)
    {
        this->printDiffTime(this->pointRefs[0], this->startRef);
        for (std::size_t i = 1; i < sz; i++)
        {
            this->printDiffTime(this->pointRefs[i], this->pointRefs[i - 1].getTime());
        }
    }
    if (this->caption.empty())
    {
        Debug::info(__FILE__, __LINE__, "elapsed time", "total: %.03f\n", diff.count());
    }
}

double Duration::checkPoint(const std::string &caption)
{
    this->pointRefs.emplace_back(caption);
}