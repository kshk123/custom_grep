#include "CustomGrep.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 5)
    {
        std::cerr << "Usage: grep_exec <query> <directory> [--ignore-case] [--regex]\n";
        return 1;
    }

    std::string           query       = argv[1];
    std::filesystem::path dirPath     = argv[2];
    bool                  ignoreCase  = false;
    bool                  useRegex    = false;

    int argIndex = 3;

    // Check for "--ignore-case"
    if (argIndex < argc && std::string(argv[argIndex]) == "--ignore-case")
    {
        ignoreCase = true;
        ++argIndex;
    }

    // Check for "--regex"
    if (argIndex < argc && std::string(argv[argIndex]) == "--regex")
    {
        useRegex = true;
        ++argIndex;
    }

    // Any remaining args are invalid
    if (argIndex < argc)
    {
        std::cerr << "Unexpected extra argument: " << argv[argIndex] << "\n";
        return 1;
    }

    try
    {
        auto all_files = cgrep::CustomGrep::collectFiles(dirPath);
        cgrep::CustomGrep custom_grep(ignoreCase, useRegex);
        auto results = custom_grep.parallelSearch(all_files, query);

        for (auto const& m : results)
        {
            std::cout << m.path.string()
                      << ":" << m.line_number
                      << ":" << m.line << "\n";
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
