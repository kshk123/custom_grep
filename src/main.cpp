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

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--ignore-case")
        {
            ignoreCase = true;
        }
        else if (arg == "--regex")
        {
            useRegex = true;
        }
        else
        {
            std::cerr << "Unrecognized option: " << arg << "\n";
            return 1;
        }
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
