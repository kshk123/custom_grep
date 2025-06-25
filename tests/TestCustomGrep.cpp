#include "CustomGrep.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// Helper: write a file with the given lines (one per line)
static void writeFile(const fs::path& path, const std::vector<std::string>& lines)
{
    std::ofstream ofs(path);
    for (auto const& l : lines)
    {
        ofs << l << "\n";
    }
    ofs.close();
    ASSERT_TRUE(fs::exists(path));
}

// Helper: recursively delete a directory if it exists
static void removeDirIfExists(const fs::path& dir)
{
    if (fs::exists(dir))
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
        ASSERT_FALSE(ec);
    }
}

TEST(CollectFiles, NestedDirectories)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_dir";
    removeDirIfExists(base);
    fs::create_directories(base / "subdir1");
    fs::create_directories(base / "subdir2" / "nested");

    writeFile(base / "root1.txt", { "hello" });
    writeFile(base / "root2.log", { "foo", "bar" });
    writeFile(base / "subdir1" / "inside1.txt", { "inside" });
    writeFile(base / "subdir1" / "inside2.txt", { "another" });
    writeFile(base / "subdir2" / "nested" / "deep.txt", { "deep" });

    auto files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(files.size(), 5u);

    std::set<fs::path> found(files.begin(), files.end());
    std::vector<fs::path> expected =
    {
        base / "root1.txt",
        base / "root2.log",
        base / "subdir1" / "inside1.txt",
        base / "subdir1" / "inside2.txt",
        base / "subdir2" / "nested" / "deep.txt"
    };
    for (auto const& p : expected)
    {
        EXPECT_TRUE(found.count(p));
    }

    removeDirIfExists(base);
}

TEST(SearchInFile, LinesContainingQuery_CaseSensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_file";
    removeDirIfExists(base);
    fs::create_directories(base);

    std::vector<std::string> lines =
    {
        "First Line",
        "Needle is here",
        "no match",
        "another Needle present",
        "needle"
    };
    auto filePath = base / "test1.txt";
    writeFile(filePath, lines);

    // Case-sensitive grep: ignoreCase=false, regexSearch=false
    cgrep::CustomGrep grep(false, false);
    auto matches = grep.searchInFile(filePath, "Needle");
    EXPECT_EQ(matches.size(), 2u);

    std::vector<size_t> expectedLines = { 2, 4 };
    std::vector<std::string> expectedContents =
    {
        "Needle is here",
        "another Needle present"
    };

    for (size_t i = 0; i < matches.size(); ++i)
    {
        EXPECT_EQ(matches[i].path, filePath);
        EXPECT_EQ(matches[i].line_number, expectedLines[i]);
        EXPECT_EQ(matches[i].line, expectedContents[i]);
    }

    // Searching for lowercase "needle" finds only line 5 in case-sensitive mode
    auto lowerMatches = grep.searchInFile(filePath, "needle");
    EXPECT_EQ(lowerMatches.size(), 1u);
    EXPECT_EQ(lowerMatches[0].line_number, 5u);
    EXPECT_EQ(lowerMatches[0].line, "needle");

    removeDirIfExists(base);
}

TEST(SearchInFile, LinesContainingQuery_CaseInsensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_file_ci";
    removeDirIfExists(base);
    fs::create_directories(base);

    std::vector<std::string> lines =
    {
        "First Line",
        "Needle is here",
        "no match",
        "another Needle present",
        "needle"
    };
    auto filePath = base / "test_ci.txt";
    writeFile(filePath, lines);

    // Case-insensitive grep: ignoreCase=true, regexSearch=false
    cgrep::CustomGrep grep(true, false);
    auto matches = grep.searchInFile(filePath, "needle");
    EXPECT_EQ(matches.size(), 3u);

    std::vector<size_t> expectedLines = { 2, 4, 5 };
    std::vector<std::string> expectedContents =
    {
        "Needle is here",
        "another Needle present",
        "needle"
    };

    for (size_t i = 0; i < matches.size(); ++i)
    {
        EXPECT_EQ(matches[i].path, filePath);
        EXPECT_EQ(matches[i].line_number, expectedLines[i]);
        EXPECT_EQ(matches[i].line, expectedContents[i]);
    }

    // Searching for uppercase "NEEDLE" also matches same lines
    auto upperMatches = grep.searchInFile(filePath, "NEEDLE");
    EXPECT_EQ(upperMatches.size(), 3u);

    removeDirIfExists(base);
}

TEST(SearchInFile, Regex_CaseSensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_regex_cs";
    removeDirIfExists(base);
    fs::create_directories(base);

    // Lines: one begins with "def", one ends with "123", one contains digits
    std::vector<std::string> lines =
    {
        "defStart",
        "Middle123",
        "no match here",
        "123end",
        "anotherdef"
    };
    auto filePath = base / "regex.txt";
    writeFile(filePath, lines);

    // Case-sensitive regex: ignoreCase=false, regexSearch=true

    // 1) '^def' should match only line 1 ("defStart")
    cgrep::CustomGrep grep_cs(false, true);
    auto matches1 = grep_cs.searchInFile(filePath, "^def");
    EXPECT_EQ(matches1.size(), 1u);
    EXPECT_EQ(matches1[0].line_number, 1u);
    EXPECT_EQ(matches1[0].line, "defStart");

    // 2) '123$' should match only line 2 ("Middle123")
    auto matches2 = grep_cs.searchInFile(filePath, "123$");
    EXPECT_EQ(matches2.size(), 1u);
    EXPECT_EQ(matches2[0].line_number, 2u);
    EXPECT_EQ(matches2[0].line, "Middle123");

    // 3) '.*def' should match any line containing "def" (case-sensitive):
    //    lines 1 ("defStart") and 5 ("anotherdef")
    auto matches3 = grep_cs.searchInFile(filePath, ".*def");
    EXPECT_EQ(matches3.size(), 2u);

    std::set<size_t> linesMatched;
    for (auto const& m : matches3)
        linesMatched.insert(m.line_number);
    EXPECT_TRUE(linesMatched.count(1));
    EXPECT_TRUE(linesMatched.count(5));

    removeDirIfExists(base);
}

TEST(SearchInFile, Regex_CaseInsensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_regex_ci";
    removeDirIfExists(base);
    fs::create_directories(base);

    std::vector<std::string> lines =
    {
        "DEFstart",
        "MidDLe456",
        "NoMatch",
        "456end",
        "anotherDEF"
    };
    auto filePath = base / "regex_ci.txt";
    writeFile(filePath, lines);

    // Case-insensitive regex: ignoreCase=true, regexSearch=true
    cgrep::CustomGrep grep_ci(true, true);

    // 1) '^def' should match only line 1 ("DEFstart")
    auto matches1 = grep_ci.searchInFile(filePath, "^def");
    EXPECT_EQ(matches1.size(), 1u);
    EXPECT_EQ(matches1[0].line_number, 1u);

    // 2) '456$' should match only line 2 ("MidDLe456")
    auto matches2 = grep_ci.searchInFile(filePath, "456$");
    EXPECT_EQ(matches2.size(), 1u);
    EXPECT_EQ(matches2[0].line_number, 2u);

    // 3) '.*def' should match lines containing "def"/"DEF": lines 1 and 5
    auto matches3 = grep_ci.searchInFile(filePath, ".*def");
    EXPECT_EQ(matches3.size(), 2u);

    std::set<size_t> linesMatched;
    for (auto const& m : matches3)
        linesMatched.insert(m.line_number);
    EXPECT_TRUE(linesMatched.count(1));
    EXPECT_TRUE(linesMatched.count(5));

    removeDirIfExists(base);
}

TEST(ParallelSearch, MultipleFiles_CaseSensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_parallel_cs";
    removeDirIfExists(base);
    fs::create_directories(base / "dirA");
    fs::create_directories(base / "dirB");

    writeFile(base / "dirA" / "A1.txt",
    {
        "apple",
        "Banana apple Cherry",
        "durian"
    });
    writeFile(base / "dirA" / "A2.txt",
    {
        "Elephant",
        "fig BANANA",
        "grape"
    });
    writeFile(base / "dirB" / "B1.txt",
    {
        "apple Banana",
        "apple apple",
        "no fruit"
    });

    auto all_files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(all_files.size(), 3u);

    cgrep::CustomGrep grep_cs(false, false);
    auto results = grep_cs.parallelSearch(all_files, "apple");

    // Case-sensitive substring "apple" appears in:
    //   dirA/A1.txt: line 1 ("apple") and line 2 ("Banana apple Cherry")
    //   dirB/B1.txt: line 1 ("apple Banana") and line 2 ("apple apple")
    EXPECT_EQ(results.size(), 4u);

    std::sort(results.begin(), results.end(),
        [](auto const& a, auto const& b)
        {
            if (a.path != b.path) return a.path < b.path;
            return a.line_number < b.line_number;
        });

    std::vector<std::pair<fs::path, size_t>> expected =
    {
        { base / "dirA" / "A1.txt", 1 },
        { base / "dirA" / "A1.txt", 2 },
        { base / "dirB" / "B1.txt", 1 },
        { base / "dirB" / "B1.txt", 2 }
    };

    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(results[i].path, expected[i].first);
        EXPECT_EQ(results[i].line_number, expected[i].second);
    }

    removeDirIfExists(base);
}

TEST(ParallelSearch, MultipleFiles_CaseInsensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_parallel_ci";
    removeDirIfExists(base);
    fs::create_directories(base / "dirA");
    fs::create_directories(base / "dirB");

    writeFile(base / "dirA" / "A1.txt",
    {
        "apple",
        "Banana apple Cherry",
        "durian"
    });
    writeFile(base / "dirA" / "A2.txt",
    {
        "Elephant",
        "fig BANANA",
        "grape"
    });
    writeFile(base / "dirB" / "B1.txt",
    {
        "apple Banana",
        "APPLE apple",
        "no fruit"
    });

    auto all_files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(all_files.size(), 3u);

    cgrep::CustomGrep grep_ci(true, false);
    auto results = grep_ci.parallelSearch(all_files, "banana");

    EXPECT_EQ(results.size(), 3u);

    std::sort(results.begin(), results.end(),
        [](auto const& a, auto const& b)
        {
            if (a.path != b.path) return a.path < b.path;
            return a.line_number < b.line_number;
        });

    std::vector<std::pair<fs::path, size_t>> expected =
    {
        { base / "dirA" / "A1.txt", 2 },
        { base / "dirA" / "A2.txt", 2 },
        { base / "dirB" / "B1.txt", 1 }
    };

    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(results[i].path, expected[i].first);
        EXPECT_EQ(results[i].line_number, expected[i].second);
    }

    removeDirIfExists(base);
}

TEST(ParallelSearch, Regex_CaseSensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_parallel_regex_cs";
    removeDirIfExists(base);
    fs::create_directories(base / "dirA");
    fs::create_directories(base / "dirB");

    writeFile(base / "dirA" / "A1.txt",
    {
        "defOne",
        "Nothing",
        "def two"
    });
    writeFile(base / "dirA" / "A2.txt",
    {
        "somethingdef",
        "abcDEF",
        "xyz"
    });
    writeFile(base / "dirB" / "B1.txt",
    {
        "defThree",
        "DEfFour",
        "last"
    });

    auto all_files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(all_files.size(), 3u);

    // Case-sensitive regex: matches lines starting exactly "def"
    cgrep::CustomGrep grep_cs(false, true);
    auto results = grep_cs.parallelSearch(all_files, "^def");

    // Should find:
    //   dirA/A1.txt: line 1 ("defOne") and line 3 ("def two")
    //   dirB/B1.txt: line 1 ("defThree")
    EXPECT_EQ(results.size(), 3u);

    std::sort(results.begin(), results.end(),
        [](auto const& a, auto const& b)
        {
            if (a.path != b.path) return a.path < b.path;
            return a.line_number < b.line_number;
        });

    std::vector<std::pair<fs::path, size_t>> expected =
    {
        { base / "dirA" / "A1.txt", 1 },
        { base / "dirA" / "A1.txt", 3 },
        { base / "dirB" / "B1.txt", 1 }
    };

    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(results[i].path, expected[i].first);
        EXPECT_EQ(results[i].line_number, expected[i].second);
    }

    removeDirIfExists(base);
}

TEST(ParallelSearch, Regex_CaseInsensitive)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_parallel_regex_ci";
    removeDirIfExists(base);
    fs::create_directories(base / "dirA");
    fs::create_directories(base / "dirB");

    writeFile(base / "dirA" / "A1.txt",
    {
        "DefOne",
        "nothing",
        "DEF two"
    });
    writeFile(base / "dirA" / "A2.txt",
    {
        "somethingdef",
        "abcDEF",
        "xyz"
    });
    writeFile(base / "dirB" / "B1.txt",
    {
        "defThree",
        "DEfFour",
        "last"
    });

    auto all_files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(all_files.size(), 3u);

    // Case-insensitive regex: matches "^def"
    cgrep::CustomGrep grep_ci(true, true);
    auto results = grep_ci.parallelSearch(all_files, "^def");

    // Should match lines starting with "def"/"DEF" ignoring case:
    //   dirA/A1.txt: line 1 ("DefOne") and line 3 ("DEF two")
    //   dirA/A2.txt: none (starts "somethingdef" not at beginning)
    //   dirB/B1.txt: line 1 ("defThree") and line 2 ("DEfFour")
    EXPECT_EQ(results.size(), 4u);

    std::sort(results.begin(), results.end(),
        [](auto const& a, auto const& b)
        {
            if (a.path != b.path) return a.path < b.path;
            return a.line_number < b.line_number;
        });

    std::vector<std::pair<fs::path, size_t>> expected =
    {
        { base / "dirA" / "A1.txt", 1 },
        { base / "dirA" / "A1.txt", 3 },
        { base / "dirB" / "B1.txt", 1 },
        { base / "dirB" / "B1.txt", 2 }
    };

    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(results[i].path, expected[i].first);
        EXPECT_EQ(results[i].line_number, expected[i].second);
    }

    removeDirIfExists(base);
}

TEST(EmptyDirectory, NoFiles)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_empty";
    removeDirIfExists(base);
    fs::create_directories(base);

    auto files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_TRUE(files.empty());

    cgrep::CustomGrep grep_ci(true, true);
    auto results = grep_ci.parallelSearch(files, "anything");
    EXPECT_TRUE(results.empty());

    removeDirIfExists(base);
}

TEST(NoMatches, SingleFile)
{
    auto base = fs::temp_directory_path() / "custom_grep_test_nomatch";
    removeDirIfExists(base);
    fs::create_directories(base);

    writeFile(base / "onlyfile.txt",
    {
        "line one",
        "line two",
        "line three"
    });

    cgrep::CustomGrep grep_ci(true, false);
    auto matches = grep_ci.searchInFile(base / "onlyfile.txt", "absent");
    EXPECT_TRUE(matches.empty());

    auto files = cgrep::CustomGrep::collectFiles(base);
    EXPECT_EQ(files.size(), 1u);

    auto results = grep_ci.parallelSearch(files, "absent");
    EXPECT_TRUE(results.empty());

    removeDirIfExists(base);
}