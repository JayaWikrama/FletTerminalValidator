#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include "fs-helper.hpp"

static const size_t copyBufferSize = 64 * 1024;

bool FSHelper::createDirectoryIfNotExists(const std::string &path)
{
    struct stat st{};

    if (stat(path.c_str(), &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return true;
        return false;
    }

    if (mkdir(path.c_str(), 0755) != 0)
        return false;

    return true;
}

bool FSHelper::copyFileInternal(const std::string &sourcePath, const std::string &destinationPath)
{
    int srcFd = open(sourcePath.c_str(), O_RDONLY);
    if (srcFd < 0)
        return false;

    struct stat st{};
    if (fstat(srcFd, &st) != 0)
    {
        close(srcFd);
        return false;
    }

    int dstFd = open(destinationPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dstFd < 0)
    {
        close(srcFd);
        return false;
    }

    char buffer[copyBufferSize];
    ssize_t bytesRead;

    while ((bytesRead = read(srcFd, buffer, sizeof(buffer))) > 0)
    {
        ssize_t totalWritten = 0;

        while (totalWritten < bytesRead)
        {
            ssize_t written = write(dstFd, buffer + totalWritten, bytesRead - totalWritten);
            if (written < 0)
            {
                close(srcFd);
                close(dstFd);
                return false;
            }
            totalWritten += written;
        }
    }

    close(srcFd);
    close(dstFd);

    if (bytesRead < 0)
        return false;

    return true;
}

bool FSHelper::hasAllowedExtension(const std::string &fileName, const std::vector<std::string> &extensions)
{
    size_t pos = fileName.rfind('.');
    if (pos == std::string::npos)
        return false;

    std::string ext = fileName.substr(pos);

    for (const auto &allowed : extensions)
    {
        if (ext == allowed)
            return true;
    }

    return false;
}

bool FSHelper::copyFile(const std::string &sourceFile, const std::string &destinationFile)
{
    return copyFileInternal(sourceFile, destinationFile);
}

bool FSHelper::removeFile(const std::string &filePath)
{
    return unlink(filePath.c_str()) == 0;
}

bool FSHelper::renameFile(const std::string &oldPath, const std::string &newPath)
{
    return rename(oldPath.c_str(), newPath.c_str()) == 0;
}

bool FSHelper::renameDirectory(const std::string &oldPath, const std::string &newPath)
{
    return rename(oldPath.c_str(), newPath.c_str()) == 0;
}

bool FSHelper::removeDirectory(const std::string &dirPath)
{
    DIR *dir = opendir(dirPath.c_str());
    if (!dir)
        return false;

    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;

        std::string fullPath = dirPath + "/" + name;

        struct stat st{};
        if (lstat(fullPath.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            if (!removeDirectory(fullPath))
            {
                closedir(dir);
                return false;
            }
        }
        else
        {
            if (unlink(fullPath.c_str()) != 0)
            {
                closedir(dir);
                return false;
            }
        }
    }

    closedir(dir);

    return rmdir(dirPath.c_str()) == 0;
}

bool FSHelper::copyDirectory(const std::string &sourceDir, const std::string &destinationDir)
{
    if (!createDirectoryIfNotExists(destinationDir))
        return false;

    DIR *dir = opendir(sourceDir.c_str());
    if (!dir)
        return false;

    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;

        std::string srcPath = sourceDir + "/" + name;
        std::string dstPath = destinationDir + "/" + name;

        struct stat st{};
        if (lstat(srcPath.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            if (!copyDirectory(srcPath, dstPath))
            {
                closedir(dir);
                return false;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (!copyFileInternal(srcPath, dstPath))
            {
                closedir(dir);
                return false;
            }
        }
    }

    closedir(dir);
    return true;
}

bool FSHelper::copyFilesWithExtensions(
    const std::string &sourceDir,
    const std::string &destinationDir,
    const std::vector<std::string> &extensions)
{
    if (!createDirectoryIfNotExists(destinationDir))
        return false;

    DIR *dir = opendir(sourceDir.c_str());
    if (!dir)
        return false;

    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;

        std::string srcPath = sourceDir + "/" + name;
        std::string dstPath = destinationDir + "/" + name;

        struct stat st{};
        if (lstat(srcPath.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            if (!copyFilesWithExtensions(srcPath, dstPath, extensions))
            {
                closedir(dir);
                return false;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (hasAllowedExtension(name, extensions))
            {
                if (!copyFileInternal(srcPath, dstPath))
                {
                    closedir(dir);
                    return false;
                }
            }
        }
    }

    closedir(dir);
    return true;
}