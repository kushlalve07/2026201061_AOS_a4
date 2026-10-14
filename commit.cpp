#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

const string DEFAULT_COMMIT_MESSAGE = "Default commit message";

// The index is a flat, sorted list of paths ("src/a.c", "src/b.c", "x.txt").
// This builds one tree per directory, deepest first, and returns the SHA of
// the tree for `prefix` ("" is the repository root).
static bool writeTreeFromIndex(const map<string, IndexEntry> &index, const string &prefix, string &sha)
{
    vector<TreeEntry> entries;
    string lastDir;

    // Every path starting with `prefix` sits in one contiguous run of the sorted map.
    for (auto it = index.lower_bound(prefix);
         it != index.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
    {
        const string rest = it->first.substr(prefix.size());
        const size_t slash = rest.find('/');

        TreeEntry entry;
        if (slash == string::npos)
        {
            entry.mode = it->second.mode;
            entry.name = rest;
            entry.sha = it->second.sha;
            entry.isDirectory = false;
        }
        else
        {
            const string dir = rest.substr(0, slash);
            if (dir == lastDir)
            {
                continue;
            }
            lastDir = dir;

            entry.mode = TREE_MODE;
            entry.name = dir;
            entry.isDirectory = true;
            if (!writeTreeFromIndex(index, prefix + dir + "/", entry.sha))
            {
                return false;
            }
        }
        entries.push_back(entry);
    }
    return storeTree(entries, sha);
}

// "Name <email>", taken from MYGIT_AUTHOR_NAME / MYGIT_AUTHOR_EMAIL if set,
// otherwise from the login name.
static string identity()
{
    const char *nameEnv = getenv("MYGIT_AUTHOR_NAME");
    if (nameEnv == nullptr || *nameEnv == '\0')
    {
        nameEnv = getenv("USER");
    }
    const string name = (nameEnv != nullptr && *nameEnv != '\0') ? nameEnv : "unknown";

    const char *emailEnv = getenv("MYGIT_AUTHOR_EMAIL");
    const string email = (emailEnv != nullptr && *emailEnv != '\0') ? emailEnv : name + "@mygit";

    return name + " <" + email + ">";
}

// "<seconds since epoch> <+hhmm timezone>", the format git stores.
static string timestamp()
{
    const time_t now = time(nullptr);
    char zone[8] = "+0000";
    const tm *local = localtime(&now);
    if (local != nullptr)
    {
        strftime(zone, sizeof(zone), "%z", local);
    }
    return to_string(static_cast<long long>(now)) + " " + zone;
}

int cmdCommit(const vector<string> &args)
{
    const string usage = "usage: ./mygit commit [-m <message>]";

    string message = DEFAULT_COMMIT_MESSAGE;
    for (size_t i = 0; i < args.size(); i++)
    {
        if (args[i] == "-m")
        {
            if (i + 1 >= args.size())
            {
                cerr << "error: -m requires a message\n"
                     << usage << endl;
                return 1;
            }
            message = args[++i];
        }
        else
        {
            cerr << "error: unexpected argument '" << args[i] << "'\n"
                 << usage << endl;
            return 1;
        }
    }
    if (message.empty())
    {
        cerr << "error: aborting commit due to empty commit message" << endl;
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
    if (index.empty())
    {
        cerr << "nothing to commit (use ./mygit add <path> to stage files)" << endl;
        return 1;
    }

    string treeSha;
    if (!writeTreeFromIndex(index, "", treeSha))
    {
        return 1;
    }

    string parentSha;
    string refPath;
    if (!readHead(parentSha, refPath))
    {
        return 1;
    }

    // A commit whose snapshot equals its parent's would record no change.
    if (!parentSha.empty())
    {
        string type;
        string content;
        Commit parent;
        if (!readObject(parentSha, type, content) || type != "commit" || !parseCommit(content, parent))
        {
            cerr << "error: HEAD points to an invalid commit " << parentSha << endl;
            return 1;
        }
        if (parent.tree == treeSha)
        {
            cerr << "nothing to commit (staged files match the last commit)" << endl;
            return 1;
        }
    }

    const string person = identity() + " " + timestamp();
    string content = "tree " + treeSha + "\n";
    if (!parentSha.empty())
    {
        content += "parent " + parentSha + "\n";
    }
    content += "author " + person + "\n";
    content += "committer " + person + "\n";
    content += "\n" + message + "\n";

    string commitSha;
    if (!hashObject("commit", content, true, commitSha) || !updateHead(commitSha))
    {
        return 1;
    }

    cout << commitSha << endl;
    return 0;
}
