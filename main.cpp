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
              << "  hash-object [-w] <file>  Compute the SHA-1 of a file and optionally store it as a blob\n"
              << "  cat-file (-p|-s|-t) <sha> Show the content, size or type of a stored object\n"
              << "  write-tree               Store the current directory as a tree object\n"
              << "  ls-tree [--name-only] <sha> List the entries of a tree object\n"
              << "  add <path>...            Stage files or directories (use . for everything)\n"
              << "  commit [-m <message>]    Record the staged files as a new commit\n"
              << "  log                      Show the commit history, newest first"
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
    if (command == "cat-file")
    {
        return cmdCatFile(args);
    }
    if (command == "write-tree")
    {
        return cmdWriteTree(args);
    }
    if (command == "ls-tree")
    {
        return cmdLsTree(args);
    }
    if (command == "add")
    {
        return cmdAdd(args);
    }
    if (command == "commit")
    {
        return cmdCommit(args);
    }
    if (command == "log")
    {
        return cmdLog(args);
    }

    cerr << "mygit: '" << command << "' is not a mygit command" << endl;
    printUsage();
    return 1;
}
