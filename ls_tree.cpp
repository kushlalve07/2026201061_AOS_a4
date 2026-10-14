#include <iostream>
using namespace std;

#include "mygit.h"

void printTree(const vector<TreeEntry> &entries, bool nameOnly)
{
    for (const TreeEntry &entry : entries)
    {
        if (nameOnly)
        {
            cout << entry.name << (entry.isDirectory ? "/" : "") << endl;
            continue;
        }

        // Directories are stored as "40000"; pad to six digits for display.
        string mode = entry.mode;
        if (mode.size() < 6)
        {
            mode.insert(0, 6 - mode.size(), '0');
        }
        cout << mode << " " << (entry.isDirectory ? "tree" : "blob") << " "
             << entry.sha << "\t" << entry.name << endl;
    }
}

int cmdLsTree(const vector<string> &args)
{
    const string usage = "usage: ./mygit ls-tree [--name-only] <tree_sha>";

    bool nameOnly = false;
    vector<string> shas;
    for (const string &arg : args)
    {
        if (arg == "--name-only")
        {
            nameOnly = true;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            cerr << "error: unknown flag '" << arg << "'\n"
                 << usage << endl;
            return 1;
        }
        else
        {
            shas.push_back(arg);
        }
    }

    if (shas.size() != 1)
    {
        cerr << usage << endl;
        return 1;
    }

    string type;
    string content;
    if (!readObject(shas[0], type, content))
    {
        return 1;
    }
    if (type != "tree")
    {
        cerr << "error: object " << shas[0] << " is a " << type << ", not a tree" << endl;
        return 1;
    }

    vector<TreeEntry> entries;
    if (!parseTree(content, entries))
    {
        cerr << "error: tree object " << shas[0] << " is corrupt" << endl;
        return 1;
    }

    printTree(entries, nameOnly);
    return 0;
}
