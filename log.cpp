#include <ctime>
#include <iostream>
using namespace std;

#include "mygit.h"

static bool isDigits(const string &text)
{
    if (text.empty())
    {
        return false;
    }
    for (char c : text)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }
    return true;
}

// Splits "Name <email> 1791964166 +0530" into "Name <email>" and a readable
// date shown in the timezone it was recorded in, e.g.
// "Wed Oct 14 13:19:26 2026 +0530".
static void splitPerson(const string &person, string &who, string &date)
{
    who = person;
    date = "unknown";

    const size_t zoneStart = person.rfind(' ');
    if (zoneStart == string::npos || zoneStart == 0)
    {
        return;
    }
    const size_t timeStart = person.rfind(' ', zoneStart - 1);
    if (timeStart == string::npos)
    {
        return;
    }

    const string seconds = person.substr(timeStart + 1, zoneStart - timeStart - 1);
    const string zone = person.substr(zoneStart + 1);
    const bool validZone = zone.size() == 5 && (zone[0] == '+' || zone[0] == '-') && isDigits(zone.substr(1));
    if (!isDigits(seconds) || seconds.size() > 15 || !validZone)
    {
        return;
    }

    who = person.substr(0, timeStart);

    // Shift by the stored offset and format as UTC, so the date reads the
    // same on every machine regardless of its local timezone.
    const long offset = (zone[0] == '-' ? -1 : 1) *
                        (stol(zone.substr(1, 2)) * 3600 + stol(zone.substr(3, 2)) * 60);
    const time_t shifted = static_cast<time_t>(stoll(seconds) + offset);
    const tm *utc = gmtime(&shifted);
    char buffer[64];
    if (utc != nullptr && strftime(buffer, sizeof(buffer), "%a %b %e %H:%M:%S %Y", utc) > 0)
    {
        date = string(buffer) + " " + zone;
    }
}

static void printCommit(const string &sha, const Commit &commit)
{
    string committer;
    string date;
    splitPerson(commit.committer, committer, date);

    cout << "commit " << sha << "\n";
    for (const string &parent : commit.parents)
    {
        cout << "Parent:    " << parent << "\n";
    }
    cout << "Committer: " << committer << "\n";
    cout << "Date:      " << date << "\n\n";

    // Indent every message line by four spaces, as git does.
    string message = commit.message;
    while (!message.empty() && message.back() == '\n')
    {
        message.pop_back();
    }
    size_t start = 0;
    while (start <= message.size())
    {
        size_t end = message.find('\n', start);
        if (end == string::npos)
        {
            end = message.size();
        }
        cout << "    " << message.substr(start, end - start) << "\n";
        start = end + 1;
    }
    cout << endl;
}

int cmdLog(const vector<string> &args)
{
    if (!args.empty())
    {
        cerr << "usage: ./mygit log" << endl;
        return 1;
    }

    string sha;
    string refPath;
    if (!readHead(sha, refPath))
    {
        return 1;
    }
    if (sha.empty())
    {
        cerr << "error: no commits yet (use ./mygit commit to create one)" << endl;
        return 1;
    }

    // Each commit names its parent, so following the chain from HEAD visits
    // the history from newest to oldest and stops at the root commit.
    while (!sha.empty())
    {
        string type;
        string content;
        if (!readObject(sha, type, content))
        {
            return 1;
        }
        Commit commit;
        if (type != "commit" || !parseCommit(content, commit))
        {
            cerr << "error: object " << sha << " is not a valid commit" << endl;
            return 1;
        }

        printCommit(sha, commit);
        sha = commit.parents.empty() ? "" : commit.parents[0];
    }
    return 0;
}
