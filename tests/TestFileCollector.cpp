#include "CustomGrep.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

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

TEST(CollectFiles, SkipsPermissionDenied)
{
    auto base = fs::temp_directory_path() / "custom_grep_perm_denied";
    removeDirIfExists(base);
    fs::create_directories(base / "allowed");
    fs::create_directories(base / "denied");

    writeFile(base / "allowed" / "file1.txt", { "ok" });
    writeFile(base / "denied" / "secret.txt", { "hidden" });

    fs::permissions(base / "denied", fs::perms::none, fs::perm_options::replace);

    testing::internal::CaptureStderr();
    auto files = cgrep::CustomGrep::collectFiles(base);
    std::string output = testing::internal::GetCapturedStderr();

    EXPECT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0], base / "allowed" / "file1.txt");
    EXPECT_NE(output.find("Permission denied"), std::string::npos);

    fs::permissions(base / "denied", fs::perms::owner_all,
                    fs::perm_options::replace);
    removeDirIfExists(base);
}

TEST(CollectFiles, ContinuesAfterPermissionDenied)
{
    auto base = fs::temp_directory_path() / "custom_grep_perm_continue";
    removeDirIfExists(base);
    fs::create_directories(base / "pre");
    fs::create_directories(base / "denied");
    fs::create_directories(base / "post");

    writeFile(base / "pre" / "a.txt", { "ok" });
    writeFile(base / "post" / "b.txt", { "ok" });
    writeFile(base / "denied" / "secret.txt", { "hidden" });

    fs::permissions(base / "denied", fs::perms::none, fs::perm_options::replace);

    testing::internal::CaptureStderr();
    auto files = cgrep::CustomGrep::collectFiles(base);
    std::string output = testing::internal::GetCapturedStderr();

    std::set<fs::path> expected = { base / "pre" / "a.txt", base / "post" / "b.txt" };
    std::set<fs::path> found(files.begin(), files.end());
    EXPECT_EQ(found, expected);
    EXPECT_NE(output.find("Permission denied"), std::string::npos);

    fs::permissions(base / "denied", fs::perms::owner_all, fs::perm_options::replace);
    removeDirIfExists(base);
}

