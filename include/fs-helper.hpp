#ifndef FSHELPER_HPP
#define FSHELPER_HPP

#include <string>
#include <vector>

class FSHelper
{
private:
    FSHelper() = delete;
    ~FSHelper() = delete;

    static bool copyFileInternal(const std::string &sourcePath, const std::string &destinationPath);
    static bool createDirectoryIfNotExists(const std::string &path);
    static bool hasAllowedExtension(const std::string &fileName, const std::vector<std::string> &extensions);

public:
    static bool copyDirectory(const std::string &sourceDir, const std::string &destinationDir);
    static bool removeDirectory(const std::string &dirPath);
    static bool renameDirectory(const std::string &oldPath, const std::string &newPath);

    static bool copyFile(const std::string &sourceFile, const std::string &destinationFile);
    static bool removeFile(const std::string &filePath);
    static bool renameFile(const std::string &oldPath, const std::string &newPath);

    static bool copyFilesWithExtensions(const std::string &sourceDir,
                                        const std::string &destinationDir,
                                        const std::vector<std::string> &extensions);
};

#endif