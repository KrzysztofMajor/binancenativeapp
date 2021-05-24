#include "stdafx.h"
#include "utils.h"

#include <numeric>

using namespace crypto;

bool utils::gzip_inflate(const std::string& compressedBytes, std::string& uncompressedBytes)
{
    if (compressedBytes.size() == 0) 
    {
        uncompressedBytes = compressedBytes;
        return true;
    }

    uncompressedBytes.clear();

    unsigned full_length = compressedBytes.size();
    unsigned half_length = compressedBytes.size() / 2;

    unsigned uncompLength = full_length;
    char* uncomp = (char*)calloc(sizeof(char), uncompLength);

    z_stream strm;
    strm.next_in = (Bytef*)compressedBytes.c_str();
    strm.avail_in = compressedBytes.size();
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;

    bool done = false;

    if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
        free(uncomp);
        return false;
    }

    while (!done) {
        // If our output buffer is too small  
        if (strm.total_out >= uncompLength) {
            // Increase size of output buffer  
            char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length);
            memcpy(uncomp2, uncomp, uncompLength);
            uncompLength += half_length;
            free(uncomp);
            uncomp = uncomp2;
        }

        strm.next_out = (Bytef*)(uncomp + strm.total_out);
        strm.avail_out = uncompLength - strm.total_out;

        // Inflate another chunk.  
        int err = inflate(&strm, Z_SYNC_FLUSH);
        if (err == Z_STREAM_END) done = true;
        else if (err != Z_OK) {
            break;
        }
    }

    if (inflateEnd(&strm) != Z_OK) {
        free(uncomp);
        return false;
    }

    for (size_t i = 0; i < strm.total_out; ++i)
        uncompressedBytes += uncomp[i];

    free(uncomp);
    return true;
}

std::string utils::random_string(size_t length)
{
    srand(time(NULL));
    auto randchar = []() -> char
    {
        const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

std::vector<std::string> utils::split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string utils::to_lower(const std::string& input)
{
    std::string lower_case = input;
    std::transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);
    return lower_case;
}

std::string utils::combine(const std::vector<std::string> values, const char* delim)
{    
    std::stringstream result;
    auto it = values.begin();
    result << *it++;
    for (; it != values.end(); it++) {
        result << delim;
        result << *it;
    }
    return result.str();
}