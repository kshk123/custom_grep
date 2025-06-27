#include "FileCollector.h"

#include <iostream>

namespace cgrep
{

void FileCollector::collectFilesRecursive(const std::filesystem::path& dir,
                                          std::vector<std::filesystem::path>& files)
{
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        std::cerr << "Permission denied, cannot access directory: "
                  << dir.string() << std::endl;
        return;
    }

    for (const auto& entry : it)
    {
        std::error_code entry_ec;
        if (entry.is_directory(entry_ec))
        {
            collectFilesRecursive(entry.path(), files);
        }
        else if (entry.is_regular_file(entry_ec))
        {
            files.push_back(entry.path());
        }

        if (entry_ec)
        {
            std::cerr << "Error accessing entry: " << entry.path().string()
                      << ": " << entry_ec.message() << std::endl;
        }
    }
}

std::vector<std::filesystem::path>
FileCollector::collectFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;

    if (std::filesystem::is_directory(dir, ec))
    {
        collectFilesRecursive(dir, files);
    }
    else if (std::filesystem::is_regular_file(dir, ec))
    {
        files.push_back(dir);
    }
    else
    {
        if (ec)
        {
            std::cerr << "Error accessing path: [" << dir.string() << "]: "
                      << ec.message() << std::endl;
        }
        else if (std::filesystem::exists(dir))
        {
            std::cerr << "Input path: [" << dir.string()
                      << "] is not a regular file or a directory." << std::endl;
        }
        else
        {
            std::cerr << "Input path: [" << dir.string() << "] does not exist." << std::endl;
        }
        return {};
    }

    if (!files.empty())
    {
        std::cout << files.size() << " files found" << std::endl;
    }

    return files;
}

} // namespace cgrep

