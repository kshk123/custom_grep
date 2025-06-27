#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cgrep
{

/// Represents a single match of `query` inside `path` at line `line_number`.
/// `line` holds the contents of that line (without the trailing newline).
struct Match
{
    std::filesystem::path path;
    size_t               line_number;
    std::string          line;
};

class CustomGrep
{
public:
    explicit CustomGrep(bool ignoreCase = false, bool regexSearch = false);
    ~CustomGrep() = default;
    CustomGrep(const CustomGrep&) = default;
    CustomGrep& operator=(const CustomGrep&) = default;
    CustomGrep(CustomGrep&&) noexcept = default;
    CustomGrep& operator=(CustomGrep&&) noexcept = default;

    /// Recursively walk `dir` and return a list of all regular files.
    /// Throws std::filesystem::filesystem_error on failure.
    static std::vector<std::filesystem::path> collectFiles(const std::filesystem::path& dir);

    /// Perform the parallel search using the number of threads set in the constructor.
    /// Each thread processes a contiguous subrange of `all_files` and calls `searchInFile`.
    [[nodiscard]] std::vector<Match> parallelSearch(const std::vector<std::filesystem::path>& all_files,
                                      const std::string& query) const;

    /// Scan the entire file at `filePath` line by line, looking for `query`.
    /// Returns a vector of Match for every line that contains `query`.
    [[nodiscard]] std::vector<Match> searchInFile(const std::filesystem::path& filePath,
                                            const std::string& query) const;

private:
    static void collectFilesRecursive(const std::filesystem::path& dir,
                               std::vector<std::filesystem::path>& files);

    void regexSearch(const std::string& query,
                     const std::filesystem::path& filePath,
                     std::ifstream& ifs,
                     std::vector<Match> &results) const;

    void regularSearch(const std::string& query,
                      const std::filesystem::path& filePath,
                      std::ifstream& ifs,
                      std::vector<Match> &results) const;

    // Number of worker threads to use. Determined in the constructor using
    // std::thread::hardware_concurrency() with a minimum of one.
    size_t m_threadCount = 1u;
    bool m_ignoreCase = false; // perform case-insensitive search if true
    bool m_regexSearch = false; // use regex search if true
};

} // namespace cgrep
