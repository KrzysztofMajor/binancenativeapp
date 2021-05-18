#pragma once

namespace binance
{
	class restful
	{
	public:
		restful();
		~restful();

		bool get(const std::string& url, std::string& result);
	private:
		bool api(const std::string& url, std::string& result);

		static constexpr const char* binance_url = "https://api.binance.com";
		CURL* curl{};
	};
}