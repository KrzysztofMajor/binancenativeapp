#include "stdafx.h"
#include "utils.h"

#include <numeric>
#include <archive.h>
#include <archive_entry.h>


using namespace crypto;

int copy_data(archive* archive, buffer_struct& uncompressedBytes)
{
    size_t size;
    const void* buff;
    int64_t offset;
    int r;
    for (;;)
    {
        r = archive_read_data_block(archive, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)        
            return (ARCHIVE_OK);        
        else if (r < ARCHIVE_OK)
            return (r);        

        uncompressedBytes.append((char*)buff, size);
    }
}

bool utils::gzip_inflate(const buffer_struct& compressedBytes, buffer_struct& uncompressedBytes)
{
    auto archive = archive_read_new();
    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);
    auto reading_result = archive_read_open_memory(archive, compressedBytes.data, compressedBytes.size_);

    archive_entry* entry;
    auto res  = archive_read_next_header(archive, &entry);

    while (res == ARCHIVE_OK) 
    {        
        res = copy_data(archive, uncompressedBytes);
        res = archive_read_next_header(archive, &(entry));        
    }

    reading_result = archive_read_free(archive);    
    return reading_result == ARCHIVE_OK;
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
    
    return fmt::format("{}{}", str, getpid());
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