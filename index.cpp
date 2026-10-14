#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

// The index is a text file with one line per staged file, sorted by path:
//   <mode> <sha> <path>
// The path is last so that it may contain spaces.
bool readIndex(map<string, IndexEntry> &index)
{
    index.clear();

    error_code ec;
    if (!fs::exists(INDEX_FILE, ec))
    {
        return true;
    }

    ifstream in(INDEX_FILE);
    if (!in)
    {
        cerr << "error: cannot read '" << INDEX_FILE << "'" << endl;
        return false;
    }

    string line;
    int lineNumber = 0;
    while (getline(in, line))
    {
        lineNumber++;
        if (line.empty())
        {
            continue;
        }

        const size_t modeEnd = line.find(' ');
        const size_t shaEnd = (modeEnd == string::npos) ? string::npos : line.find(' ', modeEnd + 1);
        if (shaEnd == string::npos || shaEnd + 1 >= line.size())
        {
            cerr << "error: index is corrupt at line " << lineNumber << endl;
            return false;
        }

        IndexEntry entry;
        entry.mode = line.substr(0, modeEnd);
        entry.sha = line.substr(modeEnd + 1, shaEnd - modeEnd - 1);
        if (!isValidSha(entry.sha))
        {
            cerr << "error: index is corrupt at line " << lineNumber << endl;
            return false;
        }
        index[line.substr(shaEnd + 1)] = entry;
    }
    return true;
}

// Writes to a temporary file first and renames it over the index, so a
// crash midway never leaves a half-written index behind.
bool writeIndex(const map<string, IndexEntry> &index)
{
    const string tempFile = INDEX_FILE + ".lock";
    ofstream out(tempFile);
    for (const auto &[path, entry] : index)
    {
        out << entry.mode << " " << entry.sha << " " << path << "\n";
    }
    out.close();
    if (!out)
    {
        cerr << "error: cannot write '" << tempFile << "'" << endl;
        return false;
    }

    error_code ec;
    fs::rename(tempFile, INDEX_FILE, ec);
    if (ec)
    {
        cerr << "error: cannot update '" << INDEX_FILE << "': " << ec.message() << endl;
        return false;
    }
    return true;
}
