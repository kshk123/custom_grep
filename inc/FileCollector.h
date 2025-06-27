#pragma once

#include <filesystem>
#include <vector>

namespace cgrep
{

class FileCollector
{
public:
    /// Recursively walk `dir` and return a list of all regular files.
    /// Throws std::filesystem::filesystem_error on failure.
    static std::vector<std::filesystem::path> collectFiles(const std::filesystem::path& dir);

private:
    static void collectFilesRecursive(const std::filesystem::path& dir,
                                      std::vector<std::filesystem::path>& files);
};

} // namespace cgrep

