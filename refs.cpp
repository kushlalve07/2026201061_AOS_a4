#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

static string trim(const string &text)
{
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos)
    {
        return "";
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

static bool writeFileAtomic(const string &path, const string &content)
{
    const string tempFile = path + ".lock";
    ofstream out(tempFile);
    out << content;
    out.close();
    if (!out)
    {
        cerr << "error: cannot write '" << tempFile << "'" << endl;
        return false;
    }

    error_code ec;
    fs::rename(tempFile, path, ec);
    if (ec)
    {
        cerr << "error: cannot update '" << path << "': " << ec.message() << endl;
        return false;
    }
    return true;
}

// HEAD is either "ref: refs/heads/<branch>" (on a branch) or a bare commit SHA
// (detached). refPath is the branch file to update, or empty when detached.
// commitSha is empty when the branch has no commits yet.
bool readHead(string &commitSha, string &refPath)
{
    commitSha.clear();
    refPath.clear();

    string head;
    if (!readFile(HEAD_FILE, head))
    {
        cerr << "error: cannot read '" << HEAD_FILE << "' (run ./mygit init first)" << endl;
        return false;
    }
    head = trim(head);

    const string prefix = "ref: ";
    if (head.rfind(prefix, 0) == 0)
    {
        refPath = MYGIT_DIR + "/" + head.substr(prefix.size());

        error_code ec;
        if (!fs::exists(refPath, ec))
        {
            return true;
        }

        string ref;
        if (!readFile(refPath, ref))
        {
            cerr << "error: cannot read '" << refPath << "'" << endl;
            return false;
        }
        commitSha = trim(ref);
        if (!isValidSha(commitSha))
        {
            cerr << "error: '" << refPath << "' does not hold a valid commit SHA" << endl;
            return false;
        }
        return true;
    }

    if (isValidSha(head))
    {
        commitSha = head;
        return true;
    }

    cerr << "error: '" << HEAD_FILE << "' is corrupt" << endl;
    return false;
}

// Moves the current branch (or HEAD itself when detached) to commitSha.
bool updateHead(const string &commitSha)
{
    string oldSha;
    string refPath;
    if (!readHead(oldSha, refPath))
    {
        return false;
    }
    if (refPath.empty())
    {
        return writeFileAtomic(HEAD_FILE, commitSha + "\n");
    }

    error_code ec;
    fs::create_directories(fs::path(refPath).parent_path(), ec);
    if (ec)
    {
        cerr << "error: cannot create '" << fs::path(refPath).parent_path().string() << "': "
             << ec.message() << endl;
        return false;
    }
    return writeFileAtomic(refPath, commitSha + "\n");
}
