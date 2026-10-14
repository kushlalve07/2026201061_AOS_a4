#include <iostream>
using namespace std;

#include "mygit.h"

int cmdCatFile(const vector<string> &args)
{
    if (args.size() != 2)
    {
        cerr << "usage: ./mygit cat-file (-p | -s | -t) <object_sha>" << endl;
        return 1;
    }

    const string &flag = args[0];
    const string &sha = args[1];
    if (flag != "-p" && flag != "-s" && flag != "-t")
    {
        cerr << "error: unknown flag '" << flag << "'\n"
             << "usage: ./mygit cat-file (-p | -s | -t) <object_sha>" << endl;
        return 1;
    }

    string type;
    string content;
    if (!readObject(sha, type, content))
    {
        return 1;
    }

    if (flag == "-t")
    {
        cout << type << endl;
    }
    else if (flag == "-s")
    {
        cout << content.size() << endl;
    }
    else if (type == "tree")
    {
        // Tree content is binary, so show it in ls-tree form instead.
        vector<TreeEntry> entries;
        if (!parseTree(content, entries))
        {
            cerr << "error: tree object " << sha << " is corrupt" << endl;
            return 1;
        }
        printTree(entries, false);
    }
    else
    {
        cout.write(content.data(), content.size());
    }
    return 0;
}
