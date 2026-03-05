#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

#include "directory-cleaner.hpp"

DirectoryCleaner::DirectoryCleaner(const std::string &path, const std::string &extension) : rootPath(path),
                                                                                            targetExtension(extension) {}

bool DirectoryCleaner::hasMatchingExtension(
    const std::string &fileName) const
{

    if (fileName.length() < targetExtension.length())
    {
        return false;
    }

    return fileName.compare(fileName.length() - targetExtension.length(),
                            targetExtension.length(),
                            targetExtension) == 0;
}

bool DirectoryCleaner::containsToken(const std::string &fileName, const std::string &contain) const
{

    return fileName.find(contain) != std::string::npos;
}

bool DirectoryCleaner::cleanRecursive(const std::string &currentPath, const std::string *contain)
{

    DIR *dir = opendir(currentPath.c_str());
    if (!dir)
    {
        return false;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr)
    {

        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        std::string fullPath = currentPath + "/" + entry->d_name;

        struct stat statbuf;

        if (lstat(fullPath.c_str(), &statbuf) != 0)
        {
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            cleanRecursive(fullPath, contain);
        }
        else if (S_ISREG(statbuf.st_mode))
        {

            std::string fileName(entry->d_name);

            if (hasMatchingExtension(fileName))
            {

                bool shouldDelete = true;

                if (contain)
                {
                    shouldDelete = containsToken(fileName, *contain);
                }

                if (shouldDelete)
                {
                    ::unlink(fullPath.c_str());
                }
            }
        }
    }

    closedir(dir);
    return true;
}

bool DirectoryCleaner::execute()
{
    return cleanRecursive(rootPath, nullptr);
}

bool DirectoryCleaner::execute(const std::string &contain)
{
    return cleanRecursive(rootPath, &contain);
}