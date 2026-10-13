#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "mygit.h"

static void printUsage()
{
    cerr << "usage: ./mygit <command> [<args>]\n"
              << "commands:\n"
              << "  init                     Create an empty repository in the current directory\n"
              << "  hash-object [-w] <file>  Compute the SHA-1 of a file and optionally store it as a blob"
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
    if (command == "hash-object")
    {
        return cmdHashObject(args);
    }

    cerr << "mygit: '" << command << "' is not a mygit command" << endl;
    printUsage();
    return 1;
}
