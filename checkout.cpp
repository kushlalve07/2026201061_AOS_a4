#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

static bool loadCommit(const string &sha, Commit &commit)
{
    string type;
    string content;
    if (!readObject(sha, type, content))
    {
        return false;
    }
    if (type != "commit")
    {
        cerr << "error: " << sha << " is a " << type << ", not a commit" << endl;
        return false;
    }
    if (!parseCommit(content, commit))
    {
        cerr << "error: commit " << sha << " is corrupt" << endl;
        return false;
    }
    return true;
}

// A name read from a tree object becomes a path on disk, so reject anything
// that could escape the working directory or touch the repository itself.
static bool isSafeName(const string &name)
{
    return !name.empty() && name != "." && name != ".." && name != MYGIT_DIR &&
           name.find('/') == string::npos;
}

// Flattens a tree into "path -> (mode, sha)" for every file beneath it,
// the same shape as the index.
static bool listTreeFiles(const string &treeSha, const string &prefix, map<string, IndexEntry> &files)
{
    string type;
    string content;
    if (!readObject(treeSha, type, content))
    {
        return false;
    }
    vector<TreeEntry> entries;
    if (type != "tree" || !parseTree(content, entries))
    {
        cerr << "error: tree " << treeSha << " is corrupt" << endl;
        return false;
    }

    for (const TreeEntry &entry : entries)
    {
        if (!isSafeName(entry.name))
        {
            cerr << "error: tree " << treeSha << " has an unsafe entry name '" << entry.name << "'" << endl;
            return false;
        }
        const string path = prefix + entry.name;
        if (entry.isDirectory)
        {
            if (!listTreeFiles(entry.sha, path + "/", files))
            {
                return false;
            }
        }
        else
        {
            files[path] = IndexEntry{entry.mode, entry.sha};
        }
    }
    return true;
}

static bool sameFiles(const map<string, IndexEntry> &a, const map<string, IndexEntry> &b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (auto itA = a.begin(), itB = b.begin(); itA != a.end(); ++itA, ++itB)
    {
        if (itA->first != itB->first || itA->second.mode != itB->second.mode ||
            itA->second.sha != itB->second.sha)
        {
            return false;
        }
    }
    return true;
}

static bool workingFileMatches(const string &path, const string &sha)
{
    error_code ec;
    if (!fs::is_regular_file(fs::symlink_status(path, ec)))
    {
        return false;
    }
    string content;
    string actual;
    return readFile(path, content) && hashObject("blob", content, false, actual) && actual == sha;
}

// True when every file inside `dir` is tracked, i.e. checkout's own cleanup
// will empty it and the directory can safely give way to a file.
static bool holdsOnlyTracked(const string &dir, const map<string, IndexEntry> &index,
                             const map<string, IndexEntry> &headFiles)
{
    error_code ec;
    fs::recursive_directory_iterator it(dir, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (fs::is_directory(fs::symlink_status(it->path(), ec)))
        {
            continue;
        }
        const string path = it->path().lexically_normal().generic_string();
        if (index.count(path) == 0 && headFiles.count(path) == 0)
        {
            return false;
        }
    }
    return !ec;
}

// Lists everything checkout would silently destroy: staged but uncommitted
// changes, edits to tracked files, and untracked files in the way.
static vector<string> findUnsavedWork(const map<string, IndexEntry> &index,
                                      const map<string, IndexEntry> &headFiles,
                                      const map<string, IndexEntry> &targetFiles)
{
    vector<string> problems;
    if (!sameFiles(index, headFiles))
    {
        problems.push_back("staged changes have not been committed");
    }
    for (const auto &[path, entry] : index)
    {
        if (!workingFileMatches(path, entry.sha))
        {
            problems.push_back("'" + path + "' has uncommitted changes");
        }
    }
    for (const auto &[path, entry] : targetFiles)
    {
        error_code ec;
        const fs::file_status status = fs::symlink_status(path, ec);
        const bool trackedDirectory = fs::is_directory(status) && holdsOnlyTracked(path, index, headFiles);
        if (index.count(path) == 0 && fs::exists(status) && !trackedDirectory &&
            !workingFileMatches(path, entry.sha))
        {
            problems.push_back("untracked '" + path + "' would be overwritten");
        }

        // An untracked file where the target needs a directory ("a" for "a/b").
        for (size_t slash = path.find('/'); slash != string::npos; slash = path.find('/', slash + 1))
        {
            const string parent = path.substr(0, slash);
            const fs::file_status parentStatus = fs::symlink_status(parent, ec);
            if (fs::exists(parentStatus) && !fs::is_directory(parentStatus) && index.count(parent) == 0 &&
                headFiles.count(parent) == 0)
            {
                problems.push_back("untracked '" + parent + "' would be overwritten");
                break;
            }
        }
    }
    return problems;
}

// After deleting a file, remove any directories it leaves empty.
static void removeEmptyParents(const fs::path &path)
{
    error_code ec;
    fs::path dir = path.parent_path();
    while (!dir.empty() && fs::is_empty(dir, ec) && !ec)
    {
        fs::remove(dir, ec);
        if (ec)
        {
            return;
        }
        dir = dir.parent_path();
    }
}

static bool restoreFile(const string &path, const IndexEntry &entry)
{
    error_code ec;
    const fs::path filePath(path);

    if (!workingFileMatches(path, entry.sha))
    {
        // A directory (or anything else) sitting where the file belongs is replaced.
        const fs::file_status status = fs::symlink_status(filePath, ec);
        if (fs::exists(status) && !fs::is_regular_file(status))
        {
            fs::remove_all(filePath, ec);
        }
        // Likewise for a file sitting where one of its parent directories belongs.
        for (size_t slash = path.find('/'); slash != string::npos; slash = path.find('/', slash + 1))
        {
            const string parent = path.substr(0, slash);
            const fs::file_status parentStatus = fs::symlink_status(parent, ec);
            if (fs::exists(parentStatus) && !fs::is_directory(parentStatus))
            {
                fs::remove(parent, ec);
            }
        }
        if (filePath.has_parent_path())
        {
            fs::create_directories(filePath.parent_path(), ec);
            if (ec)
            {
                cerr << "error: cannot create directory for '" << path << "': " << ec.message() << endl;
                return false;
            }
        }

        string type;
        string content;
        if (!readObject(entry.sha, type, content))
        {
            return false;
        }
        if (type != "blob")
        {
            cerr << "error: object " << entry.sha << " for '" << path << "' is not a blob" << endl;
            return false;
        }

        ofstream out(path, ios::binary | ios::trunc);
        out.write(content.data(), content.size());
        out.close();
        if (!out)
        {
            cerr << "error: cannot write '" << path << "'" << endl;
            return false;
        }
    }

    const fs::perms execute = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
    fs::permissions(filePath, execute,
                    entry.mode == "100755" ? fs::perm_options::add : fs::perm_options::remove, ec);
    return true;
}

int cmdCheckout(const vector<string> &args)
{
    const string usage = "usage: ./mygit checkout [-f] (<commit_sha> | <branch>)";

    bool force = false;
    vector<string> targets;
    for (const string &arg : args)
    {
        if (arg == "-f")
        {
            force = true;
        }
        else
        {
            targets.push_back(arg);
        }
    }
    if (targets.size() != 1)
    {
        cerr << usage << endl;
        return 1;
    }

    error_code ec;
    if (!fs::is_directory(OBJECTS_DIR, ec))
    {
        cerr << "error: not a mygit repository (run ./mygit init first)" << endl;
        return 1;
    }

    // The target is either a full commit SHA (detached HEAD) or a branch name.
    const string &target = targets[0];
    string targetSha;
    string branch;
    if (isValidSha(target))
    {
        targetSha = target;
    }
    else if (isSafeName(target) && fs::is_regular_file(HEADS_DIR + "/" + target, ec))
    {
        branch = target;
        string ref;
        readFile(HEADS_DIR + "/" + branch, ref);
        targetSha = ref.substr(0, ref.find_first_of(" \r\n"));
        if (!isValidSha(targetSha))
        {
            cerr << "error: branch '" << branch << "' does not hold a valid commit SHA" << endl;
            return 1;
        }
    }
    else
    {
        cerr << "error: '" << target << "' is not a valid commit SHA or branch name" << endl;
        return 1;
    }

    Commit targetCommit;
    map<string, IndexEntry> targetFiles;
    if (!loadCommit(targetSha, targetCommit) || !listTreeFiles(targetCommit.tree, "", targetFiles))
    {
        return 1;
    }

    // What is tracked right now: the index and the commit HEAD points to.
    string headSha;
    string refPath;
    map<string, IndexEntry> index;
    map<string, IndexEntry> headFiles;
    if (!readHead(headSha, refPath) || !readIndex(index))
    {
        return 1;
    }
    if (!headSha.empty())
    {
        Commit headCommit;
        if (!loadCommit(headSha, headCommit) || !listTreeFiles(headCommit.tree, "", headFiles))
        {
            return 1;
        }
    }

    if (!force)
    {
        const vector<string> problems = findUnsavedWork(index, headFiles, targetFiles);
        if (!problems.empty())
        {
            cerr << "error: checkout would discard unsaved work:" << endl;
            for (const string &problem : problems)
            {
                cerr << "  " << problem << endl;
            }
            cerr << "commit your changes first, or use ./mygit checkout -f to discard them" << endl;
            return 1;
        }
    }

    // 1. Delete tracked files that do not exist in the target snapshot.
    map<string, IndexEntry> tracked = headFiles;
    tracked.insert(index.begin(), index.end());
    for (const auto &[path, entry] : tracked)
    {
        if (targetFiles.count(path) == 0 && fs::is_regular_file(fs::symlink_status(path, ec)))
        {
            fs::remove(path, ec);
            if (ec)
            {
                cerr << "error: cannot remove '" << path << "': " << ec.message() << endl;
                return 1;
            }
            removeEmptyParents(path);
        }
    }

    // 2. Write every file of the target snapshot from its blob.
    for (const auto &[path, entry] : targetFiles)
    {
        if (!restoreFile(path, entry))
        {
            return 1;
        }
    }

    // 3. The index now mirrors the checked-out commit, and HEAD points at it.
    if (!writeIndex(targetFiles))
    {
        return 1;
    }
    if (!setHead(branch.empty() ? targetSha : "ref: refs/heads/" + branch))
    {
        return 1;
    }

    string summary = targetCommit.message.substr(0, targetCommit.message.find('\n'));
    if (branch.empty())
    {
        cout << "HEAD is now at " << targetSha << " " << summary << endl;
    }
    else
    {
        cout << "Switched to branch '" << branch << "' (" << targetSha << " " << summary << ")" << endl;
    }
    return 0;
}
