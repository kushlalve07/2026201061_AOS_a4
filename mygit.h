#ifndef MYGIT_H
#define MYGIT_H

#include <string>
#include <vector>

const std::string MYGIT_DIR = ".mygit";
const std::string OBJECTS_DIR = ".mygit/objects";
const std::string REFS_DIR = ".mygit/refs";
const std::string HEADS_DIR = ".mygit/refs/heads";
const std::string HEAD_FILE = ".mygit/HEAD";
const std::string INDEX_FILE = ".mygit/index";

const std::string DEFAULT_BRANCH = "main";

bool readFile(const std::string &path, std::string &content);
std::string sha1Hex(const std::string &data);
std::string objectPath(const std::string &sha);
bool hashObject(const std::string &type, const std::string &content, bool write, std::string &sha);

int cmdInit(const std::vector<std::string> &args);
int cmdHashObject(const std::vector<std::string> &args);

#endif
