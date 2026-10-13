#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <openssl/sha.h>
#include <zlib.h>
using namespace std;

#include "mygit.h"

namespace fs = filesystem;

bool readFile(const string &path, string &content)
{
    ifstream in(path, ios::binary);
    if (!in)
    {
        return false;
    }
    content.assign(istreambuf_iterator<char>(in), istreambuf_iterator<char>());
    return !in.bad();
}

string sha1Hex(const string &data)
{
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(), digest);

    const char hexDigits[] = "0123456789abcdef";
    string hex;
    hex.reserve(2 * SHA_DIGEST_LENGTH);
    for (unsigned char byte : digest)
    {
        hex.push_back(hexDigits[byte >> 4]);
        hex.push_back(hexDigits[byte & 0x0f]);
    }
    return hex;
}

static bool compressData(const string &data, string &compressed)
{
    uLongf compressedSize = compressBound(data.size());
    compressed.resize(compressedSize);
    int status = compress(reinterpret_cast<Bytef *>(&compressed[0]), &compressedSize,
                          reinterpret_cast<const Bytef *>(data.data()), data.size());
    if (status != Z_OK)
    {
        return false;
    }
    compressed.resize(compressedSize);
    return true;
}

string objectPath(const string &sha)
{
    return OBJECTS_DIR + "/" + sha.substr(0, 2) + "/" + sha.substr(2);
}

// An object is stored as "<type> <size>\0<content>". The SHA-1 is taken over
// this whole string, and the zlib-compressed form is what goes on disk.
bool hashObject(const string &type, const string &content, bool write, string &sha)
{
    string object = type + " " + to_string(content.size());
    object.push_back('\0');
    object += content;

    sha = sha1Hex(object);
    if (!write)
    {
        return true;
    }

    error_code ec;
    if (!fs::is_directory(OBJECTS_DIR, ec))
    {
        cerr << "error: not a mygit repository (run ./mygit init first)" << endl;
        return false;
    }

    const string path = objectPath(sha);
    if (fs::exists(path, ec))
    {
        return true;
    }

    string compressed;
    if (!compressData(object, compressed))
    {
        cerr << "error: failed to compress object " << sha << endl;
        return false;
    }

    fs::create_directories(fs::path(path).parent_path(), ec);
    if (ec)
    {
        cerr << "error: cannot create object directory: " << ec.message() << endl;
        return false;
    }

    ofstream out(path, ios::binary);
    out.write(compressed.data(), compressed.size());
    out.close();
    if (!out)
    {
        cerr << "error: cannot write object file '" << path << "'" << endl;
        return false;
    }
    return true;
}
