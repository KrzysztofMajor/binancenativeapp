#include "stdafx.h"
#include "binance.h"
#include "restful.h"


class CurlGlobalStateGuard
{
public:
    CurlGlobalStateGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalStateGuard() { curl_global_cleanup(); }
};

void connectionLost(void* context, char* cause)
{
    spdlog::error(cause);
}

int mqtt_arrived_cb(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    return 1;
}

class kline
{
public:
    kline(MQTTClient& mosq, binance::api& api)
        : mosq_(mosq), api_(api)
    {

    }

    void subscribe(const std::string& symbol_name)
    {
        std::string lower_case = symbol_name;
        std::transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);

        auto channel = fmt::format("/ws/{}@kline_15m", lower_case);
        auto topic = fmt::format("{}/kline_15m", lower_case);
        api_.connect_endpoint(channel, [&, topic](std::string_view& value)
        {
            rapidjson::Document json_result{};
            json_result.Parse(value.data());            
            
            rapidjson::Document mqtt_result{};
            mqtt_result.SetObject();

            auto& allocator = mqtt_result.GetAllocator();
            
            mqtt_result.AddMember("event_time", json_result["E"].GetUint64(), allocator);

            auto& kline = json_result["k"];
            mqtt_result.AddMember("start_time", kline["t"].GetUint64(), allocator);
            mqtt_result.AddMember("end_time", kline["T"].GetUint64(), allocator);
            
            mqtt_result.AddMember("open_price", rapidjson::StringRef(kline["o"].GetString()), allocator);
            mqtt_result.AddMember("close_price", rapidjson::StringRef(kline["c"].GetString()), allocator);
            
            mqtt_result.AddMember("high_price", rapidjson::StringRef(kline["h"].GetString()), allocator);
            mqtt_result.AddMember("low_price", rapidjson::StringRef(kline["l"].GetString()), allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            mqtt_result.Accept(writer);

            const char* str = buffer.GetString();
            MQTTClient_publish(mosq_, topic.c_str(), strlen(str), str, 0, 0, nullptr);
        });
    }
private:
    MQTTClient& mosq_;
    binance::api& api_;
};

class ticker
{
public:
    ticker(MQTTClient& mosq, binance::api& api)
        : mosq_(mosq), api_(api), start_{ std::chrono::steady_clock::now() }
    {

    }

    void subscribe(const std::string& symbol_name)
    {        
        std::string lower_case = symbol_name;
        std::transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);
        
        auto channel = fmt::format("/ws/{}@bookTicker", lower_case);
        auto topic = fmt::format("{}/top_of_book", lower_case);
        api_.connect_endpoint(channel, [&, topic](std::string_view& value)
            {                    
                auto current = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current - start_).count();
                if (elapsed >= 1)
                {
                    spdlog::info("web socket data");
                    start_ = current;
                }

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
private:
    MQTTClient& mosq_;
    binance::api& api_;

    std::chrono::steady_clock::time_point start_;
};

std::string random_string(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

int main(int argc, char** argv)
{        
    CurlGlobalStateGuard handle_curl_state{};
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

    //binance::restful rest{};
    //rest.get_file("https://data.binance.vision/data/spot/monthly/klines/BTCUSDT/12h/BTCUSDT-12h-2021-04.zip");

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    auto client_id = random_string(5);
    MQTTClient_create(&client, host, client_id.c_str(),  MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    int rc;    
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to connect, return code {}", rc);
        return EXIT_FAILURE;
    }

    rc = MQTTClient_setCallbacks(client, nullptr,  connectionLost, mqtt_arrived_cb, nullptr);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to set callback, return code {}", rc);
        return EXIT_FAILURE;
    }

    binance::api api{ cert };
    //api.get_exchange_info();

    ticker ticker_(client, api);
    //ticker_.subscribe("DOGEUSDT");
    //ticker_.subscribe("ETHUSDT");
    ticker_.subscribe("BTCUSDT");

    kline kline_(client, api);
    kline_.subscribe("BTCUSDT");    
    
    auto start = std::chrono::steady_clock::now();
    while (1)
    {
        auto current = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
        if (elapsed >= 1)
        {
            spdlog::info("event_loop");
            start = current;
        }

        api.event_loop();        
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}