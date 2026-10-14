#include <filesystem>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

// Turns a user-supplied path into a clean path relative to the repository
// root ("./src//a.txt" -> "src/a.txt"), rejecting anything outside it.
static bool normalizePath(const string &arg, string &relative)
{
    fs::path path(arg);
    if (path.is_absolute())
    {
        path = path.lexically_relative(fs::current_path());
    }
    string normalized = path.lexically_normal().generic_string();
    while (normalized.size() > 1 && normalized.back() == '/')
    {
        normalized.pop_back();
    }

    if (normalized.empty() || normalized == ".." || normalized.rfind("../", 0) == 0)
    {
        cerr << "error: '" << arg << "' is outside the repository" << endl;
        return false;
    }
    if (normalized == MYGIT_DIR || normalized.rfind(MYGIT_DIR + "/", 0) == 0)
    {
        cerr << "error: '" << arg << "' is inside the repository's internal directory" << endl;
        return false;
    }
    relative = normalized;
    return true;
}

static bool isUnder(const string &path, const string &dir)
{
    return dir == "." || path == dir || path.rfind(dir + "/", 0) == 0;
}

static bool stageFile(const string &path, const fs::file_status &status, map<string, IndexEntry> &index)
{
    string content;
    if (!readFile(path, content))
    {
        cerr << "error: cannot read '" << path << "'" << endl;
        return false;
    }

    IndexEntry entry;
    if (!hashObject("blob", content, true, entry.sha))
    {
        return false;
    }
    const bool executable = (status.permissions() & fs::perms::owner_exec) != fs::perms::none;
    entry.mode = executable ? "100755" : "100644";
    index[path] = entry;
    return true;
}

static bool stageDirectory(const string &dir, map<string, IndexEntry> &index)
{
    error_code ec;
    fs::recursive_directory_iterator it(dir, ec);
    if (ec)
    {
        cerr << "error: cannot open directory '" << dir << "': " << ec.message() << endl;
        return false;
    }

    // Symlinked directories are not followed (the iterator's default), and
    // symlinks or special files are skipped, matching write-tree.
    for (; it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        const string path = it->path().lexically_normal().generic_string();
        if (path == MYGIT_DIR)
        {
            it.disable_recursion_pending();
            continue;
        }

        const fs::file_status status = fs::symlink_status(it->path(), ec);
        if (ec)
        {
            cerr << "error: cannot stat '" << path << "': " << ec.message() << endl;
            return false;
        }
        if (fs::is_regular_file(status) && !stageFile(path, status, index))
        {
            return false;
        }
    }
    if (ec)
    {
        cerr << "error: cannot read directory '" << dir << "': " << ec.message() << endl;
        return false;
    }

    // Files that were staged earlier but have since been deleted are unstaged.
    for (auto entry = index.begin(); entry != index.end();)
    {
        if (isUnder(entry->first, dir) && !fs::is_regular_file(fs::symlink_status(entry->first, ec)))
        {
            entry = index.erase(entry);
        }
        else
        {
            ++entry;
        }
    }
    return true;
}

// A path that no longer exists on disk stages its removal, as long as
// something under that path was tracked in the index.
static bool stageRemoval(const string &path, const string &arg, map<string, IndexEntry> &index)
{
    bool removed = false;
    for (auto entry = index.begin(); entry != index.end();)
    {
        if (isUnder(entry->first, path))
        {
            entry = index.erase(entry);
            removed = true;
        }
        else
        {
            ++entry;
        }
    }
    if (!removed)
    {
        cerr << "error: pathspec '" << arg << "' did not match any files" << endl;
    }
    return removed;
}

int cmdAdd(const vector<string> &args)
{
    if (args.empty())
    {
        cerr << "usage: ./mygit add <path>...\n"
             << "       ./mygit add ." << endl;
        return 1;
    }

    error_code ec;
    if (!fs::is_directory(OBJECTS_DIR, ec))
    {
        cerr << "error: not a mygit repository (run ./mygit init first)" << endl;
        return 1;
    }

    map<string, IndexEntry> index;
    if (!readIndex(index))
    {
        return 1;
    }

    // All changes are made in memory and written once at the end, so a
    // failure on any path leaves the index exactly as it was.
    for (const string &arg : args)
    {
        string path;
        if (!normalizePath(arg, path))
        {
            return 1;
        }

        const fs::file_status status = fs::symlink_status(path, ec);
        bool ok;
        if (fs::is_directory(status))
        {
            ok = stageDirectory(path, index);
        }
        else if (fs::is_regular_file(status))
        {
            ok = stageFile(path, status, index);
        }
        else if (!fs::exists(status))
        {
            ok = stageRemoval(path, arg, index);
        }
        else
        {
            cerr << "error: '" << arg << "' is not a regular file or directory" << endl;
            ok = false;
        }

        if (!ok)
        {
            return 1;
        }
    }

    return writeIndex(index) ? 0 : 1;
}
