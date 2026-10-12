#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "mygit.h"

static void printUsage()
{
    cerr << "usage: ./mygit <command> [<args>]\n"
              << "commands:\n"
              << "  init    Create an empty repository in the current directory"
              << endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    const string command = argv[1];
    const vector<string> args(argv + 2, argv + argc);

    if (command == "init")
    {
        return cmdInit(args);
    }

    cerr << "mygit: '" << command << "' is not a mygit command" << endl;
    printUsage();
    return 1;
}
