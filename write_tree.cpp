#include <filesystem>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

// Stores `dir` as a tree object (children first) and returns its SHA.
// isEmpty tells the caller that the directory holds nothing trackable.
static bool writeTree(const fs::path &dir, string &sha, bool &isEmpty)
{
    vector<TreeEntry> entries;

    error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec)
    {
        cerr << "error: cannot open directory '" << dir.string() << "': " << ec.message() << endl;
        return false;
    }

    for (; it != fs::directory_iterator(); it.increment(ec))
    {
        const fs::path path = it->path();
        const string name = path.filename().string();
        if (name == MYGIT_DIR)
        {
            continue;
        }

        // symlink_status does not follow links, so symlinks and special files are skipped.
        const fs::file_status status = fs::symlink_status(path, ec);
        if (ec)
        {
            cerr << "error: cannot stat '" << path.string() << "': " << ec.message() << endl;
            return false;
        }

        TreeEntry entry;
        entry.name = name;
        if (fs::is_directory(status))
        {
            bool childEmpty = false;
            if (!writeTree(path, entry.sha, childEmpty))
            {
                return false;
            }
            if (childEmpty)
            {
                continue;
            }
            entry.mode = TREE_MODE;
            entry.isDirectory = true;
        }
        else if (fs::is_regular_file(status))
        {
            string content;
            if (!readFile(path.string(), content))
            {
                cerr << "error: cannot read '" << path.string() << "'" << endl;
                return false;
            }
            if (!hashObject("blob", content, true, entry.sha))
            {
                return false;
            }
            const bool executable = (status.permissions() & fs::perms::owner_exec) != fs::perms::none;
            entry.mode = executable ? "100755" : "100644";
            entry.isDirectory = false;
        }
        else
        {
            continue;
        }
        entries.push_back(entry);
    }
    if (ec)
    {
        cerr << "error: cannot read directory '" << dir.string() << "': " << ec.message() << endl;
        return false;
    }

    isEmpty = entries.empty();
    return storeTree(entries, sha);
}

int cmdWriteTree(const vector<string> &args)
{
    if (!args.empty())
    {
        cerr << "usage: ./mygit write-tree" << endl;
        return 1;
    }

    error_code ec;
    if (!fs::is_directory(OBJECTS_DIR, ec))
    {
        cerr << "error: not a mygit repository (run ./mygit init first)" << endl;
        return 1;
    }

    string sha;
    bool isEmpty = false;
    if (!writeTree(".", sha, isEmpty))
    {
        return 1;
    }

    cout << sha << endl;
    return 0;
}
