#include "stdafx.h"
#include "binance.h"

void my_log_callback(mosquitto* mosq, void* userdata, int level, const char* str)
{    
    spdlog::trace("{}", str);
}

void my_message_callback(mosquitto* mosq, void* userdata, const mosquitto_message* message)
{
    if (message->payloadlen) 
    {
        spdlog::info("{} {}", message->topic, message->payload);
    }
    else {
        spdlog::info("{} (null)", message->topic);
    }    
}

void my_subscribe_callback(struct mosquitto* mosq, void* userdata, int mid, int qos_count, const int* granted_qos)
{
    spdlog::info("Subscribed (mid: {}): {}", mid, qos_count);    
}

void my_connect_callback(struct mosquitto* mosq, void* userdata, int result)
{    
    if (!result)             
        mosquitto_subscribe(mosq, NULL, "test/#", 2);    
    else
        spdlog::error("Connect failed");    
}

class ticker
{
public:
    ticker(mosquitto* mosq, binance::api& api)
        : mosq_(mosq), api_(api)
    {

    }

    void subscribe(const std::string& symbol_name)
    {
        auto symbol = api_.get_symbol(symbol_name);
        if (symbol)
        {
            std::string lower_case = symbol_name;
            std::transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);

            auto channel = fmt::format("/ws/{}@bookTicker", lower_case);
            auto topic = fmt::format("top_of_book/{}", lower_case);
            api_.connect_endpoint(channel, [&, topic](std::string_view& value)
                {                    
                    rapidjson::Document json_result{};
                    json_result.Parse(value.data());
                    auto u = json_result["u"].GetUint64();

                    auto bid_price = json_result["b"].GetString();
                    auto bid_qty = json_result["B"].GetString();
                    auto ask_price = json_result["a"].GetString();
                    auto ask_qty = json_result["A"].GetString();

                    rapidjson::Document mqtt_result{};
                    mqtt_result.SetObject();

                    auto& allocator = mqtt_result.GetAllocator();                    
                    
                    rapidjson::Value bid_price_val(rapidjson::kObjectType);
                    bid_price_val.SetString(bid_price, strlen(bid_price), allocator);
                    mqtt_result.AddMember("bid_price", bid_price_val, allocator);

                    rapidjson::Value bid_qty_val(rapidjson::kObjectType);
                    bid_qty_val.SetString(bid_qty, strlen(bid_qty), allocator);
                    mqtt_result.AddMember("bid_qty", bid_qty_val, allocator);

                    rapidjson::Value ask_price_val(rapidjson::kObjectType);
                    ask_price_val.SetString(ask_price, strlen(ask_price), allocator);
                    mqtt_result.AddMember("ask_price", ask_price_val, allocator);

                    rapidjson::Value ask_qty_val(rapidjson::kObjectType);
                    ask_qty_val.SetString(ask_qty, strlen(ask_qty), allocator);
                    mqtt_result.AddMember("ask_qty", ask_qty_val, allocator);

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    mqtt_result.Accept(writer);

                    const char* str = buffer.GetString();
                    mosquitto_publish(mosq_, nullptr, topic.c_str(), strlen(str), str, 0, false);
                });
        }
    }
private:
    mosquitto* mosq_{};
    binance::api& api_;
};

int main(int argc, char** argv)
{        
    spdlog::info("Welcome to binancenativeapp!");

    cxxopts::Options options("binancenativeapp", "binancenativeapp");
    options.add_options()
        ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
        ("h,host", "Multicast group", cxxopts::value<std::string>()->default_value("192.168.1.45"))
        ("p,port", "Multicast port", cxxopts::value<int>()->default_value("1883"))
        ("k,keepalive", "Enable debugging", cxxopts::value<int>()->default_value("60"))
        ("c,cert", "cert file", cxxopts::value<std::string>()->default_value("c:/project/client/binancenativeapp/cacert.pem"));

    auto result = options.parse(argc, argv);

    bool debug = result["debug"].as<bool>();
    int port = result["port"].as<int>();
    const char* host = result["host"].as<std::string>().c_str();
    const char* cert = result["cert"].as<std::string>().c_str();
    int keepalive = result["keepalive"].as<int>();


    mosquitto_lib_init();
    auto mosq = mosquitto_new(NULL, true, NULL);

    if (mosquitto_connect(mosq, host, port, keepalive)) 
    {        
        spdlog::error("Unable to connect: {}:{}", host, port);
        return 1;
    }       

    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

    binance::api api{ cert };
    api.get_exchange_info();

    ticker ticker_(mosq, api);
    ticker_.subscribe("DOGEUSDT");
    ticker_.subscribe("BTCUSDT");
    ticker_.subscribe("ETHUSDT");
    
    while (1)
    {
        api.event_loop();
        mosquitto_loop(mosq, 0, 1);
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();    
}