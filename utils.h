#pragma once

namespace crypto
{
	struct utils
	{
		static bool gzip_inflate(const std::string& compressedBytes, std::string& uncompressedBytes);
		static std::string random_string(size_t length);
		static std::vector<std::string> split(const std::string& s, char delimiter);
		static std::string to_lower(const std::string& input);
		static std::string combine(const std::vector<std::string> values, const char* delim);
	};
}