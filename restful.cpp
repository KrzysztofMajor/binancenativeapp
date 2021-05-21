#include "stdafx.h"
#include "restful.h"

using namespace binance;

static size_t curl_cb(void* content, size_t size, size_t nmemb, std::string* buffer)
{
	buffer->append((char*)content, size * nmemb);
	return size * nmemb;
}

size_t write_to_file(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realsize = size * nmemb;
	auto file = reinterpret_cast<std::ofstream*>(userp);
	file->write(reinterpret_cast<const char*>(contents), realsize);
	return realsize;
}

restful::restful()
{
	curl = curl_easy_init();	
}

restful::~restful()
{
	curl_easy_cleanup(curl);	
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

timeval get_timeout(CURLM* multi_handle)
{
    long curl_timeo = -1;
    /* set a suitable timeout to play around with */
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    curl_multi_timeout(multi_handle, &curl_timeo);
    if (curl_timeo >= 0) {
        timeout.tv_sec = curl_timeo / 1000;
        if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;
        else
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }
    return timeout;
}

int wait_if_needed(CURLM* multi_handle, timeval& timeout)
{
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);
    int maxfd = -1;
    /* get file descriptors from the transfers */
    auto mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

    if (mc != CURLM_OK) {
        std::cerr << "curl_multi_fdset() failed, code " << mc << "." << std::endl;
    }
    /* On success the value of maxfd is guaranteed to be >= -1. We call
           sleep for 100ms, which is the minimum suggested value in the
           curl_multi_fdset() doc. */
    /*if (maxfd == -1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }*/

    int rc = maxfd != -1 ? select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout) : 0;
    return rc;
}

void multi_loop(CURLM* multi_handle)
{
    int still_running = 0; /* keep number of running handles */

    /* we start some action by calling perform right away */
    curl_multi_perform(multi_handle, &still_running);

    while (still_running) 
    {
        struct timeval timeout = get_timeout(multi_handle);

        auto rc = wait_if_needed(multi_handle, timeout);

        if (rc >= 0)
        {
            /* timeout or readable/writable sockets */
            curl_multi_perform(multi_handle, &still_running);
        }
        /* else select error */
    }
}

void restful::get_file(const std::string& url)
{
	using MultiHandle = std::unique_ptr<CURLM, std::function<void(CURLM*)>>;
	using EasyHandle = std::unique_ptr<CURL, std::function<void(CURL*)>>;

	MultiHandle multi_handle = MultiHandle(curl_multi_init(), curl_multi_cleanup);
	auto handle = EasyHandle(curl_easy_init(), curl_easy_cleanup);

	curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());	
	curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L);

	std::ofstream file_stream("downloaded_data.txt", std::ios::binary);
	curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, write_to_file);
	curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, reinterpret_cast<void*>(&file_stream));
		
	curl_multi_add_handle(multi_handle.get(), handle.get());

    multi_loop(multi_handle.get());

	curl_multi_remove_handle(multi_handle.get(), handle.get());
}