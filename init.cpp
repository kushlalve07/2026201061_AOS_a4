#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

int cmdInit(const vector<string> &args)
{
    if (!args.empty())
    {
        cerr << "usage: ./mygit init" << endl;
        return 1;
    }

    error_code ec;

    if (fs::exists(MYGIT_DIR, ec))
    {
        if (fs::is_directory(MYGIT_DIR, ec))
        {
            cout << "Repository already initialized in "
                      << fs::absolute(MYGIT_DIR).string() << endl;
            return 0;
        }
        cerr << "error: '" << MYGIT_DIR << "' exists and is not a directory" << endl;
        return 1;
    }

    for (const string &dir : {OBJECTS_DIR, HEADS_DIR})
    {
        fs::create_directories(dir, ec);
        if (ec)
        {
            cerr << "error: cannot create directory '" << dir << "': "
                      << ec.message() << endl;
            return 1;
        }
    }

    ofstream head(HEAD_FILE);
    head << "ref: refs/heads/" << DEFAULT_BRANCH << "\n";
    head.close();
    if (!head)
    {
        cerr << "error: cannot write '" << HEAD_FILE << "'" << endl;
        return 1;
    }

    ofstream index(INDEX_FILE);
    index.close();
    if (!index)
    {
        cerr << "error: cannot write '" << INDEX_FILE << "'" << endl;
        return 1;
    }

    cout << "Initialized empty mygit repository in " << fs::absolute(MYGIT_DIR).string() << endl;
    return 0;
}
