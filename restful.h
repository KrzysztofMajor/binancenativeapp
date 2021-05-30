#pragma once

namespace binance
{	
	class restful
	{
	public:
		restful();
		~restful();

		bool get(const std::string& url, std::string& result, std::string& header_result);
		static crypto::buffer_struct get_file(const std::string& url);
	private:
		bool api(const std::string& url, std::string& result, std::string& header_result);

		static constexpr const char* binance_url = "https://api.binance.com";
		CURL* curl{};
	};
}