#include "stdafx.h"
#include "restful.h"

using namespace binance;

static size_t curl_cb(void* content, size_t size, size_t nmemb, std::string* buffer)
{
	buffer->append((char*)content, size * nmemb);
	return size * nmemb;
}

restful::restful()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
}

restful::~restful()
{
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}

bool restful::get(const std::string& url, std::string& result)
{
	std::string binanceUrl{ binance_url };
	binanceUrl += url;

	return api(binanceUrl, result);
}

bool restful::api(const std::string& url, std::string& str_result)
{
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str_result);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");

	auto res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		spdlog::error("<restful::api> curl_easy_perform() failed: {}", curl_easy_strerror(res));		
		return false;
	}
	
	return res == CURLE_OK;
}