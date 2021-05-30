#pragma once

namespace crypto
{
	struct buffer_struct
	{
		char* data{};
		size_t size_{};
		size_t max_size_{};

		buffer_struct() = default;

		buffer_struct(const buffer_struct& other)
		{
			size_ = other.size_;
			max_size_ = other.max_size_;

			data = new char[max_size_];
			memcpy(data, other.data, other.size_);
		}

		void append(const char* ptr, size_t size)
		{
			size_t required_size = size_ + size;
			if (required_size > max_size_)
			{
				max_size_ = required_size * 2;
				auto b = new char[max_size_];
				memcpy(b, data, size_);
				delete[] data;
				data = b;
			}
			memcpy(data + size_, ptr, size);
			size_ += size;
		}

		~buffer_struct()
		{
			delete[] data;
			max_size_ = 0;
			size_ = 0;
		}
	};

	struct utils
	{
		static bool gzip_inflate(const buffer_struct& compressedBytes, buffer_struct& uncompressedBytes);
		static std::string random_string(size_t length);
		static std::vector<std::string> split(const std::string& s, char delimiter);
		static std::string to_lower(const std::string& input);
		static std::string combine(const std::vector<std::string> values, const char* delim);
	};
}