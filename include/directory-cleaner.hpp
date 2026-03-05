#ifndef __DIRECTORY_CLEANER_HPP__
#define __DIRECTORY_CLEANER_HPP__

#include <string>

class DirectoryCleaner
{
private:
    std::string rootPath;
    std::string targetExtension;

    bool hasMatchingExtension(const std::string &fileName) const;
    bool containsToken(const std::string &fileName, const std::string &contain) const;

    bool cleanRecursive(const std::string &currentPath, const std::string *contain);

public:
    DirectoryCleaner(const std::string &path, const std::string &extension);

    bool execute();
    bool execute(const std::string &contain);
};

#endif