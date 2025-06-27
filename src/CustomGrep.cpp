#include "CustomGrep.h"

#include <fstream>
#include <thread>
#include <algorithm>
#include <regex>
#include <iostream>
#include <system_error>

namespace cgrep
{

// Helper: lowercase an ASCII string in place.
static void lowercaseInPlace(std::string& s)
{
    std::transform(
        s.begin(), s.end(),
        s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); }
    );
}

CustomGrep::CustomGrep(bool ignoreCase, bool regexSearch)
    : m_ignoreCase(ignoreCase)
    , m_regexSearch(regexSearch)
{
    // Determine how many threads to use. std::thread::hardware_concurrency()
    // may return 0 if the value is not well defined on a system. In that
    // case fall back to 1 thread.
    auto hc = std::thread::hardware_concurrency();
    m_threadCount = (hc == 0) ? 1u : static_cast<size_t>(hc);
}

// private recursive helper function
void CustomGrep::collectFilesRecursive(const std::filesystem::path& dir, std::vector<std::filesystem::path>& files)
{
    std::error_code ec;
    // This is where we attempt to access the directory. If we lack permission, `ec` will be set.
    std::filesystem::directory_iterator it(dir, ec);

    if (ec)
    {
        // Log the permission error and return. This stops recursion down this path
        // but allows the caller to continue with other directories.
        std::cerr << "Permission denied, cannot access directory: " << dir.string() << std::endl;
        return;
    }

    for (const auto& entry : it)
    {
        std::error_code entry_ec;
        if (entry.is_directory(entry_ec))
        {
            // It's a directory, so we recurse.
            collectFilesRecursive(entry.path(), files);
        }
        else if (entry.is_regular_file(entry_ec))
        {
            files.push_back(entry.path());
        }

        if (entry_ec)
        {
            // An error occurred checking the entry type.
            // Log it and continue with the next entry in the current directory.
            std::cerr << "Error accessing entry: " << entry.path().string() << ": " << entry_ec.message() << std::endl;
        }
    }
}

// This function goes through a directory recursively and collects all regular files.
// The input is a directory path, and it returns a vector of paths to the files found in that directory
// (and its subdirectories).
std::vector<std::filesystem::path>
CustomGrep::collectFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;

    // First, check if the path is a directory.
    if (std::filesystem::is_directory(dir, ec))
    {
        collectFilesRecursive(dir, files);
    }
    // If not a directory, check if it's a regular file.
    else if (std::filesystem::is_regular_file(dir, ec))
    {
        files.push_back(dir);
    }
    // If it's neither, or an error occurred, report it.
    else
    {
        if (ec)
        {
            std::cerr << "Error accessing path: [" << dir.string() << "]: " << ec.message() << std::endl;
        }
        else if (std::filesystem::exists(dir))
        {
            std::cerr << "Input path: [" << dir.string() << "] is not a regular file or a directory." << std::endl;
        }
        else
        {
            std::cerr << "Input path: [" << dir.string() << "] does not exist." << std::endl;
        }
        return {}; // Return empty vector
    }

    if (!files.empty())
    {
        std::cout << files.size() << " files found" << std::endl;
    }
    return files;
}

// parallelSearch: perform the parallel search using the number of threads set in the constructor.
// Each thread processes a contiguous subrange of `all_files` and calls `searchInFile`.
// Every thread will handle a chunk of files, and the results will be collected and merged at the end.
// Since each thread processes a different subrange of files and writes to its own local vector,
// we do not need any synchronization.
// All threads will write to their own vector, and we will merge them at the end.
std::vector<Match> CustomGrep::parallelSearch(
    const std::vector<std::filesystem::path>& all_files,
    const std::string& query
) const
{
    size_t total_files = all_files.size();
    if (total_files == 0 || m_threadCount == 0)
    {
        std::cerr << "No files to search or no threads available." << std::endl;
        return {}; // nothing to scan
    }

    // Compute how many files each thread will process (ceiling division)
    size_t chunk_size = (total_files + m_threadCount - 1) / m_threadCount;

    // Prepare per-thread storage for results
    std::vector<std::vector<Match>> local_results(m_threadCount);

    // Launch exactly m_threadCount threads, each handling its subrange
    std::vector<std::thread> threads;
    threads.reserve(m_threadCount);

    for (size_t thread_index = 0; thread_index < m_threadCount; ++thread_index)
    {
        size_t start_idx = thread_index * chunk_size;
        if (start_idx >= total_files)
        {
            // More threads than files: this (and subsequent) thread has no work
            std::cout << "Thread " << thread_index
                      << " has no files to process." << std::endl;
            break;
        }
        size_t end_idx = std::min(start_idx + chunk_size, total_files);

        threads.emplace_back([&, thread_index, start_idx, end_idx]
        {
            auto& out = local_results[thread_index];

            for (size_t path_index = start_idx; path_index < end_idx; ++path_index)
            {
                const auto& path = all_files[path_index];
                auto matches = searchInFile(path, query);
                if (!matches.empty())
                {
                    out.insert(
                        out.end(),
                        std::make_move_iterator(matches.begin()),
                        std::make_move_iterator(matches.end())
                    );
                }
            }
        });
    }

    // Join all threads
    for (auto& th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    // Merge per-thread results into a single vector
    std::vector<Match> all_results;
    size_t total_matches = 0;
    for (auto& vec : local_results)
    {
        total_matches += vec.size();
    }
    all_results.reserve(total_matches);

    for (auto& vec : local_results)
    {
        all_results.insert(
            all_results.end(),
            std::make_move_iterator(vec.begin()),
            std::make_move_iterator(vec.end())
        );
    }

    return all_results;
}

// searchInFile: scan the entire file at `filePath` line by line, looking for `query`.
// Returns a vector of Match for every line that contains `query`.
std::vector<Match> CustomGrep::searchInFile(const std::filesystem::path& filePath,
                                            const std::string& query) const
{
    std::vector<Match> results;
    std::ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        std::error_code ec(errno, std::generic_category());

        if (ec == std::errc::permission_denied)
        {
            std::cerr << "Permission denied, cannot access file: " << filePath.string() << "\n";
        }
        else
        {
            std::cerr << "Could not open file [" << filePath.string() << "]: " << ec.message() << "\n";
        }
        return results;
    }
    if (m_regexSearch)
    {
        // Compile regex once, with icase if requested
        regexSearch(query, filePath, ifs, results);
    }
    else
    {
        // Regular search: case-sensitive or case-insensitive
        regularSearch(query, filePath, ifs, results);
    }
    return results;
}

void CustomGrep::regexSearch(const std::string& query,
                             const std::filesystem::path& filePath,
                             std::ifstream& ifs,
                             std::vector<Match> &results) const
{
    // Compile regex once, with icase if requested
    std::regex_constants::syntax_option_type flags =
        std::regex_constants::ECMAScript;
    if (m_ignoreCase)
    {
        flags = flags | std::regex_constants::icase;
    }

    std::regex re(query, flags);

    std::string line;
    size_t      lineNumber = 0;
    while (std::getline(ifs, line))
    {
        if (!line.empty() && line.back() == '\r') // to handle Windows-style line endings
        {
            line.pop_back();
        }
        ++lineNumber;
        if (std::regex_search(line, re))
        {
            results.push_back(Match{filePath, lineNumber, line});
        }
    }
}

void CustomGrep::regularSearch(const std::string& query,
                               const std::filesystem::path& filePath,
                               std::ifstream& ifs,
                               std::vector<Match> &results) const
{
    std::string line;
    size_t      lineNumber = 0;

    // For case-insensitive search, convert the query string to lowercase
    std::string lowerQuery;
    if (m_ignoreCase)
    {
        lowerQuery = query;
        lowercaseInPlace(lowerQuery);
    }

    while (std::getline(ifs, line))
    {
        if (!line.empty() && line.back() == '\r') // to handle Windows-style line endings
        {
            line.pop_back();
        }
        ++lineNumber;
        if (m_ignoreCase)
        {
            // For case-insensitive search, convert the line to lowercase
            std::string lowerLine = line;
            lowercaseInPlace(lowerLine);
            if (lowerLine.find(lowerQuery) != std::string::npos)
            {
                results.push_back(Match{filePath, lineNumber, line});
            }
        }
        else
        {
            if (line.find(query) != std::string::npos)
            {
                results.push_back(Match{filePath, lineNumber, line});
            }
        }
    }
}

} // namespace cgrep
