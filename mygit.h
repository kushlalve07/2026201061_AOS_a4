#ifndef MYGIT_H
#define MYGIT_H

#include <map>
#include <string>
#include <vector>

const std::string MYGIT_DIR = ".mygit";
const std::string OBJECTS_DIR = ".mygit/objects";
const std::string REFS_DIR = ".mygit/refs";
const std::string HEADS_DIR = ".mygit/refs/heads";
const std::string HEAD_FILE = ".mygit/HEAD";
const std::string INDEX_FILE = ".mygit/index";

const std::string DEFAULT_BRANCH = "main";

const std::string TREE_MODE = "40000";

struct TreeEntry
{
    std::string mode;
    std::string name;
    std::string sha;
    bool isDirectory;
};

struct Commit
{
    std::string tree;
    std::vector<std::string> parents;
    std::string author;
    std::string committer;
    std::string message;
};

// One staged file. The index maps each repo-relative path to its entry.
struct IndexEntry
{
    std::string mode;
    std::string sha;
};

bool readFile(const std::string &path, std::string &content);
std::string toHex(const std::string &raw);
std::string fromHex(const std::string &hex);
std::string sha1Hex(const std::string &data);
std::string objectPath(const std::string &sha);
bool hashObject(const std::string &type, const std::string &content, bool write, std::string &sha);
bool isValidSha(const std::string &sha);
bool readObject(const std::string &sha, std::string &type, std::string &content);
bool parseTree(const std::string &content, std::vector<TreeEntry> &entries);
void printTree(const std::vector<TreeEntry> &entries, bool nameOnly);
bool storeTree(std::vector<TreeEntry> entries, std::string &sha);
bool parseCommit(const std::string &content, Commit &commit);
bool readHead(std::string &commitSha, std::string &refPath);
bool updateHead(const std::string &commitSha);
bool readIndex(std::map<std::string, IndexEntry> &index);
bool writeIndex(const std::map<std::string, IndexEntry> &index);

int cmdInit(const std::vector<std::string> &args);
int cmdHashObject(const std::vector<std::string> &args);
int cmdCatFile(const std::vector<std::string> &args);
int cmdWriteTree(const std::vector<std::string> &args);
int cmdLsTree(const std::vector<std::string> &args);
int cmdAdd(const std::vector<std::string> &args);
int cmdCommit(const std::vector<std::string> &args);
int cmdLog(const std::vector<std::string> &args);

#endif
