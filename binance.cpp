#include "stdafx.h"
#include "binance.h"

using namespace binance;

#define BINANCE_WS_HOST "stream.binance.com"
#define BINANCE_WS_PORT 9443
#define BINANCE_WS_PROTOCOL_NAME "example-protocol"

api::api(const char* cert)
{	
	lws_context_creation_info info;
	memset(&info, 0, sizeof(info));

	struct lws_protocols protocols[] =
	{
		{ BINANCE_WS_PROTOCOL_NAME, api::event_cb, 0, 65536 },
		{ NULL, NULL, 0, 0 } /* terminator */
	};

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.user = this;
	info.gid = -1;
	info.uid = -1;
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.client_ssl_ca_filepath = cert;// "cacert.pem";

	context_  = lws_create_context(&info);
}

api::~api()
{
	lws_context_destroy(context_);
}

int api::event_cb_impl(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	switch (reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lws_callback_on_writable(wsi);
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
	{				
		std::string_view message((char*)in, len);
		subscribers_[subscriber_callbacks_[wsi]].callback_func(message);		
		break;
	}
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		break;
	case LWS_CALLBACK_CLOSED:
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	{
		subscribers_[subscriber_callbacks_[wsi]].wsi = nullptr;
		subscriber_callbacks_.erase(wsi);

		spdlog::warn("LWS_CALLBACK_CLOSED");
		break;
	}		
	}
	return 0;
}

int api::event_cb(lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len)
{
	auto* userdata = (api*)lws_context_user(lws_get_context(wsi));	
	return userdata->event_cb_impl(wsi, reason, user, in, len);
}

void api::connect_endpoint(const std::string& path, const callback& func)
{		
	subscribers_.emplace_back(subscriber{ subscribers_.size(), nullptr, path, func });
	auto& c = subscribers_.back();
	connect_endpoint(c);
}

void api::connect_endpoint(subscriber& s)
{
	lws_client_connect_info ccinfo = { 0 };
	ccinfo.context = context_;
	ccinfo.address = BINANCE_WS_HOST;
	ccinfo.port = BINANCE_WS_PORT;
	ccinfo.path = s.path.c_str();
	ccinfo.host = lws_canonical_hostname(context_);
	ccinfo.origin = "origin";
	ccinfo.protocol = BINANCE_WS_PROTOCOL_NAME;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

	ccinfo.pwsi = &s.wsi;
	lws_client_connect_via_info(&ccinfo);
	subscriber_callbacks_[s.wsi] = s.index;
	s.subscribe_ts = std::chrono::steady_clock::now();
}

void api::event_loop()
{
	for (auto& subscriber : subscribers_)
	{
		if (subscriber.wsi == nullptr)
		{
			auto now = std::chrono::steady_clock::now();			
			uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(now - subscriber.subscribe_ts).count();
			if (seconds > 2)
			{
				spdlog::warn("reconecting");
				connect_endpoint(subscriber);
			}
		}
	}

	lws_service(context_, 500);
}

bool api::get(const std::string& url, const callback& func)
{	
	std::string str_result;
	if (!rest.get(url, str_result))
	{
		spdlog::error("api::get: unable to get '{}'", url);
		return false;
	}
		

	auto begin = str_result.c_str();
	auto end = begin + str_result.length();
	
	std::string_view message(str_result);
	func(message);

	return true;
}

bool api::get(const std::string& url, const std::string& query_string, const callback& func)
{
	std::string u(url);
	u.append(query_string);
	return get(u, func);
}

currency& api::get_currency(const std::string& name)
{
	auto i = currencies_.find(name);
	if (i == std::end(currencies_))	
		i = currencies_.emplace_hint(i, name, currency{ name });

	return i->second;
}

symbol* api::get_symbol(const std::string& name)
{
	auto i = symbols_.find(name);
	if (i == std::end(symbols_))
		return nullptr;
	else
		return &i->second;
}

void api::get_exchange_info()
{
	symbols_.clear();
	
	get("/api/v3/exchangeInfo", [&](std::string_view& value)
	{
		rapidjson::Document json_result{};
		json_result.Parse(value.data());

		const std::string name = json_result["timezone"].GetString();
		auto serverTime = json_result["serverTime"].GetUint64();

		for (auto& symbolJson : json_result["symbols"].GetArray())
		{
			const std::string symbol_string = symbolJson["symbol"].GetString();
			const std::string status = symbolJson["status"].GetString();

			const std::string baseAsset = symbolJson["baseAsset"].GetString();
			const auto baseAssetPrecision = symbolJson["baseAssetPrecision"].GetUint64();

			const std::string quoteAsset = symbolJson["quoteAsset"].GetString();
			const auto quoteAssetPrecision = symbolJson["quoteAssetPrecision"].GetUint64();

			auto& base = get_currency(baseAsset);
			auto& quote = get_currency(quoteAsset);

			order_types orderTypes{ order_types::NONE };
			for (auto& orderType : symbolJson["orderTypes"].GetArray())
			{
				auto type = std::string(orderType.GetString());
				if (type == "LIMIT")
					orderTypes = order_types(orderTypes | order_types::LIMIT);
				else if (type == "LIMIT_MAKER")
					orderTypes = order_types(orderTypes | order_types::LIMIT_MAKER);
				else if (type == "MARKET")
					orderTypes = order_types(orderTypes | order_types::MARKET);
				else if (type == "STOP_LOSS")
					orderTypes = order_types(orderTypes | order_types::STOP_LOSS);
				else if (type == "STOP_LOSS_LIMIT")
					orderTypes = order_types(orderTypes | order_types::STOP_LOSS_LIMIT);
				else if (type == "TAKE_PROFIT")
					orderTypes = order_types(orderTypes | order_types::TAKE_PROFIT);
				else if (type == "TAKE_PROFIT_LIMIT")
					orderTypes = order_types(orderTypes | order_types::TAKE_PROFIT_LIMIT);
			}

			for (auto& filter : symbolJson["filters"].GetArray())
			{
				auto type = std::string(filter["filterType"].GetString());
				if (type == "PRICE_FILTER")
				{

				}
				else if(type == "PERCENT_PRICE")
				{

				}
				else if (type == "LOT_SIZE")
				{

				}
				else if (type == "MIN_NOTIONAL")
				{

				}
				else if (type == "ICEBERG_PARTS")
				{

				}
				else if (type == "MARKET_LOT_SIZE")
				{

				}
				else if (type == "MAX_NUM_ORDERS")
				{

				}
				else if (type == "MAX_NUM_ALGO_ORDERS")
				{

				}
				else if (type == "MAX_NUM_ICEBERG_ORDERS")
				{

				}
				else if (type == "MAX_POSITION FILTER")
				{

				}
			}

			symbol s{ symbol_string, base, quote, orderTypes };
			s.base_asset_precision() = baseAssetPrecision;
			s.quote_asset_precision() = quoteAssetPrecision;

			symbols_.emplace(std::make_pair(symbol_string, s));
		}
	});
}

void api::get_depth(const std::string& symbol, const callback& func)
{	
	std::string querystring("symbol=");
	querystring.append(symbol);
	querystring.append("&limit=1000");

	get("/api/v3/depth?", querystring, func);
}
