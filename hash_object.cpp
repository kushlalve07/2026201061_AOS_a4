#include <filesystem>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

int cmdHashObject(const vector<string> &args)
{
    bool write = false;
    vector<string> files;
    for (const string &arg : args)
    {
        if (arg == "-w")
        {
            write = true;
        }
        else
        {
            files.push_back(arg);
        }
    }

    if (files.size() != 1)
    {
        cerr << "usage: ./mygit hash-object [-w] <file>" << endl;
        return 1;
    }

    const string &file = files[0];
    error_code ec;
    if (!fs::exists(file, ec))
    {
        cerr << "error: '" << file << "' does not exist" << endl;
        return 1;
    }
    if (!fs::is_regular_file(file, ec))
    {
        cerr << "error: '" << file << "' is not a regular file" << endl;
        return 1;
    }

    string content;
    if (!readFile(file, content))
    {
        cerr << "error: cannot read '" << file << "'" << endl;
        return 1;
    }

    string sha;
    if (!hashObject("blob", content, write, sha))
    {
        return 1;
    }

    cout << sha << endl;
    return 0;
}
