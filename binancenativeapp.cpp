#include "stdafx.h"
#include "binance.h"

class ticker
{
public:
    ticker(MQTTClient& mosq, binance::api& api)
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
                    MQTTClient_publish(mosq_, topic.c_str(), strlen(str), str, 0, 0, nullptr);
                });
        }
    }
private:
    MQTTClient& mosq_;
    binance::api& api_;
};

int main(int argc, char** argv)
{        
    spdlog::info("Welcome to binancenativeapp!");

    cxxopts::Options options("binancenativeapp", "binancenativeapp");
    options.add_options()
        ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
        ("h,host", "Multicast group", cxxopts::value<std::string>()->default_value("tcp://192.168.1.45:1883"))
        ("p,port", "Multicast port", cxxopts::value<int>()->default_value("1883"))
        ("k,keepalive", "Enable debugging", cxxopts::value<int>()->default_value("60"))
        ("c,cert", "cert file", cxxopts::value<std::string>()->default_value("c:/project/client/binancenativeapp/cacert.pem"));

    auto result = options.parse(argc, argv);

    bool debug = result["debug"].as<bool>();    
    const char* host = result["host"].as<std::string>().c_str();
    const char* cert = result["cert"].as<std::string>().c_str();
    int keepalive = result["keepalive"].as<int>();


    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    MQTTClient_create(&client, host, "binancenativeapp", MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    int rc;    
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to connect, return code {}", rc);
        return EXIT_FAILURE;
    }

    binance::api api{ cert };
    api.get_exchange_info();

    ticker ticker_(client, api);
    ticker_.subscribe("DOGEUSDT");
    ticker_.subscribe("BTCUSDT");
    ticker_.subscribe("ETHUSDT");
    
    while (1)
    {
        api.event_loop();        
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}