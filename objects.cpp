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

string toHex(const string &raw)
{
    const char hexDigits[] = "0123456789abcdef";
    string hex;
    hex.reserve(2 * raw.size());
    for (unsigned char byte : raw)
    {
        hex.push_back(hexDigits[byte >> 4]);
        hex.push_back(hexDigits[byte & 0x0f]);
    }
    return hex;
}

static int hexValue(char c)
{
    return (c >= '0' && c <= '9') ? c - '0' : c - 'a' + 10;
}

// Expects a string that already passed isValidSha.
string fromHex(const string &hex)
{
    string raw;
    raw.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        raw.push_back(static_cast<char>((hexValue(hex[i]) << 4) | hexValue(hex[i + 1])));
    }
    return raw;
}

string sha1Hex(const string &data)
{
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(), digest);
    return toHex(string(reinterpret_cast<const char *>(digest), SHA_DIGEST_LENGTH));
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

static bool decompressData(const string &compressed, string &data)
{
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK)
    {
        return false;
    }
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
    stream.avail_in = compressed.size();

    // The original size is not known up front, so inflate in fixed-size chunks.
    char buffer[16384];
    int status = Z_OK;
    data.clear();
    while (status == Z_OK)
    {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        status = inflate(&stream, Z_NO_FLUSH);
        data.append(buffer, sizeof(buffer) - stream.avail_out);
    }
    inflateEnd(&stream);
    return status == Z_STREAM_END;
}

bool isValidSha(const string &sha)
{
    if (sha.size() != 2 * SHA_DIGEST_LENGTH)
    {
        return false;
    }
    for (char c : sha)
    {
        bool isHexDigit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!isHexDigit)
        {
            return false;
        }
    }
    return true;
}

string objectPath(const string &sha)
{
    return OBJECTS_DIR + "/" + sha.substr(0, 2) + "/" + sha.substr(2);
}

bool readObject(const string &sha, string &type, string &content)
{
    error_code ec;
    if (!fs::is_directory(OBJECTS_DIR, ec))
    {
        cerr << "error: not a mygit repository (run ./mygit init first)" << endl;
        return false;
    }
    if (!isValidSha(sha))
    {
        cerr << "error: '" << sha << "' is not a valid 40-character SHA-1 hash" << endl;
        return false;
    }

    const string path = objectPath(sha);
    if (!fs::is_regular_file(path, ec))
    {
        cerr << "error: object " << sha << " not found" << endl;
        return false;
    }

    string compressed;
    string object;
    if (!readFile(path, compressed) || !decompressData(compressed, object))
    {
        cerr << "error: cannot read object " << sha << endl;
        return false;
    }

    // Split "<type> <size>\0<content>" and cross-check it against the SHA.
    const size_t headerEnd = object.find('\0');
    const size_t space = object.find(' ');
    if (headerEnd == string::npos || space == string::npos || space > headerEnd)
    {
        cerr << "error: object " << sha << " is corrupt" << endl;
        return false;
    }
    type = object.substr(0, space);
    content = object.substr(headerEnd + 1);

    const string size = object.substr(space + 1, headerEnd - space - 1);
    if (size != to_string(content.size()) || sha1Hex(object) != sha)
    {
        cerr << "error: object " << sha << " is corrupt" << endl;
        return false;
    }
    return true;
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

// Tree content is a sequence of "<mode> <name>\0<20 raw SHA bytes>" entries.
bool parseTree(const string &content, vector<TreeEntry> &entries)
{
    entries.clear();
    size_t pos = 0;
    while (pos < content.size())
    {
        const size_t space = content.find(' ', pos);
        if (space == string::npos)
        {
            return false;
        }
        const size_t nameEnd = content.find('\0', space + 1);
        if (nameEnd == string::npos || nameEnd + 1 + SHA_DIGEST_LENGTH > content.size())
        {
            return false;
        }

        TreeEntry entry;
        entry.mode = content.substr(pos, space - pos);
        entry.name = content.substr(space + 1, nameEnd - space - 1);
        entry.sha = toHex(content.substr(nameEnd + 1, SHA_DIGEST_LENGTH));
        entry.isDirectory = (entry.mode == TREE_MODE);
        entries.push_back(entry);

        pos = nameEnd + 1 + SHA_DIGEST_LENGTH;
    }
    return true;
}
