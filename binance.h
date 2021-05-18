#pragma once

#include "restful.h"

namespace binance
{
	enum order_types : uint32_t
	{
		NONE = 0,
		LIMIT = 1 << 0,
		LIMIT_MAKER = 1 << 1,
		MARKET = 1 << 2,
		STOP_LOSS = 1 << 3,
		STOP_LOSS_LIMIT = 1 << 4,
		TAKE_PROFIT = 1 << 5,
		TAKE_PROFIT_LIMIT = 1 << 6
	};

	class currency
	{
	public:
		currency(const std::string& name)
			: name_(name)
		{

		}

		const std::string& get_name() const
		{
			return name_;
		}
	protected:
		std::string name_;
	};

	class symbol
	{
	public:
		symbol(const std::string& name, currency& base, currency& quote, order_types order_types)
			: name_(name), base_(base), quote_(quote), order_types_(order_types)
		{

		}

		const std::string& get_name() const
		{
			return name_;
		}

		uint64_t& base_asset_precision()
		{
			return baseAssetPrecision_;
		}

		uint64_t base_asset_precision() const
		{
			return baseAssetPrecision_;
		}

		uint64_t& quote_asset_precision()
		{
			return quoteAssetPrecision_;
		}

		uint64_t quote_asset_precision() const
		{
			return quoteAssetPrecision_;
		}
	protected:
		std::string name_;

		currency& base_;
		currency& quote_;
		order_types order_types_;

		uint64_t baseAssetPrecision_;
		uint64_t quoteAssetPrecision_;
	};

	class trade
	{
	public:
		trade(const rapidjson::Value& value)
		{
			/*id_ = value["t"].asUInt64();
			price_ = std::stod(value["p"].asString());
			quantity_ = std::stod(value["q"].asString());
			mm_ = value["m"].asBool();*/
		}

		uint64_t id() const { return id_; };
		double price() const { return price_; };
		double quantity() const { return quantity_; };
		bool mm() const { return mm_; };
	private:
		uint64_t id_;
		double price_;
		double quantity_;
		bool mm_;
	};
}

template <> struct fmt::formatter<binance::trade> : formatter<string_view> 
{	
	template <typename FormatContext>
	auto format(binance::trade c, FormatContext& ctx) 
	{		
		return formatter<string_view>::format(fmt::format("id:{}, price:{}, quantity:{}, mm:{}", c.id(), c.price(), c.quantity(), c.mm()), ctx);
	}
};
	
namespace binance
{
	class api
	{
	public:
		using callback = std::function<void(std::string_view&)>;

		api(const char* cert);
		~api();

		void get_exchange_info();		

		symbol* get_symbol(const std::string& name);

		void get_depth(const std::string& symbol, const callback& func);

		void connect_endpoint(const std::string& path, const callback& func);
		void event_loop();
	private:
		static int event_cb(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
		int event_cb_impl(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
		
		bool get(const std::string& url, const callback& func);
		bool get(const std::string& url, const std::string& query_string, const callback& func);

		//Json::CharReader* reader_{};
		restful rest{};
		lws_context* context_{};

		std::unordered_map<std::string, currency> currencies_;
		std::unordered_map<std::string, symbol> symbols_;
				
		struct subscriber
		{
			uint64_t index;
			lws* wsi;			
			std::string path;
			callback callback_func;			

			std::chrono::time_point<std::chrono::steady_clock> subscribe_ts{ std::chrono::steady_clock::now() };
		};

		void connect_endpoint(subscriber& s);
		
		std::unordered_map<lws*, uint64_t> subscriber_callbacks_;
		std::vector<subscriber> subscribers_;

		currency& get_currency(const std::string& name);		
	};
}