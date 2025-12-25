#ifndef __DURATION_HPP__
#define __DURATION_HPP__

#include <chrono>
#include <vector>
#include <string>

class Duration
{
private:
    class PointRefs
    {
    private:
        std::chrono::steady_clock::time_point timePoint;
        std::string caption;

    public:
        PointRefs(const std::string &caption = "");
        ~PointRefs();

        const std::chrono::steady_clock::time_point &getTime() const;
        const std::string &getCaption() const;

        double diff(const std::chrono::steady_clock::time_point &ref) const;
    };

    std::chrono::steady_clock::time_point startRef;
    std::vector<PointRefs> pointRefs;
    std::string caption;

    void printDiffTime(const PointRefs &pref, const std::chrono::steady_clock::time_point &sref) const;

public:
    Duration(const std::string &caption = "");
    ~Duration();

    void checkPoint(const std::string &caption = "");
    double getTotalDurationInSeconds() const;
    int getTotalDurationInMs() const;
};

#endif