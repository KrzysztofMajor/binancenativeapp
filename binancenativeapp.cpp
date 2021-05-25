#include "stdafx.h"
#include "binance.h"
#include "restful.h"

#include "ticker.h"

class CurlGlobalStateGuard
{
public:
    CurlGlobalStateGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalStateGuard() { curl_global_cleanup(); }
};

void connectionLost(void* context, char* cause)
{
    if (cause)
        spdlog::error(cause);
    else
        spdlog::error("connectionLost");
}

void delivered(void* context, MQTTClient_deliveryToken dt)
{    
    
}

moodycamel::ConcurrentQueue<std::tuple<std::string, std::string>> q(1024);

int mqtt_arrived_cb(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    std::string topic(topicName);

    std::string payload((char*)message->payload, message->payloadlen);    
    q.enqueue(std::make_tuple(topic, payload));
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
template<typename T>
using handle = std::unique_ptr<T, std::function<void(T*)>>;

int main(int argc, char** argv)
{        
    CurlGlobalStateGuard handle_curl_state{};    
    cxxopts::Options options("binancenativeapp", "binancenativeapp");
    options.add_options()        
        ("n,name", "name", cxxopts::value<std::string>()->default_value("binancenativeapp"))
        ("h,host", "Multicast group", cxxopts::value<std::string>()->default_value("tcp://192.168.1.45:1883"))                
        ("c,cert", "cert file", cxxopts::value<std::string>()->default_value("c:/project/client/binancenativeapp/cacert.pem"))        
        ("s,symbols", "Multicast group", cxxopts::value<std::string>()->default_value("ethbtc;ltcbtc;bnbbtc"));

    auto result = options.parse(argc, argv);
        
    const char* host = result["host"].as<std::string>().c_str();    
    const char* cert = result["cert"].as<std::string>().c_str();    
    
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    auto client_id = crypto::utils::random_string(5);
    spdlog::info("Welcome to binancenativeapp: {}", client_id);

    spdlog::info("binancenativerecorder: MQTTClient_create");
    MQTTClient_create(&client, host, client_id.c_str(),  MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    spdlog::info("binancenativerecorder: MQTTClient_setCallbacks");
    int rc = MQTTClient_setCallbacks(client, nullptr, connectionLost, mqtt_arrived_cb, delivered);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to set callback, return code {}", rc);
        return EXIT_FAILURE;
    }

    spdlog::info("binancenativerecorder: MQTTClient_connect");
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to connect, return code {}", rc);
        return EXIT_FAILURE;
    }        

    auto name = result["name"].as<std::string>();
    auto topic = fmt::format("strategy_control/{}/control", name);
    int qos = 1;
    spdlog::info("binancenativerecorder: MQTTClient_subscribe");
    rc = MQTTClient_subscribe(client, topic.c_str(), qos);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to subscrib for '{}', return code {}", topic, rc);
        return EXIT_FAILURE;
    }

    binance::api api{ cert };
    //api.get_exchange_info();
        
    const auto symbols = result["symbols"].as<std::string>();
    if (symbols.size())
    {
        auto pairs = crypto::utils::split(symbols, ';');

        binance::ticker ticker_(client, api);
        ticker_.subscribe(pairs, "bookTicker");
        ticker_.subscribe(pairs, "aggTrade");
    }
       
    auto ping_topic = fmt::format("strategy_control/{}/ping", name);
    auto start = std::chrono::system_clock::now();
    bool run = true;
    do
    {
        api.event_loop();

        auto current = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
        if (elapsed >= 1)
        {
            unsigned __int64 epoch = std::chrono::duration_cast<std::chrono::milliseconds>(current.time_since_epoch()).count();
            std::string data = fmt::format("{}", epoch);
            MQTTClient_publish(client, ping_topic.c_str(), data.length(), data.c_str(), 0, 0, nullptr);
            start = current;
        }

        std::tuple<std::string, std::string> payload;
        if (q.try_dequeue(payload))
        {
            rapidjson::Document json_result{};
            json_result.Parse(std::get<1>(payload).data());            

            if (json_result.HasMember("command"))
            {
                auto command = json_result["command"].GetString();
                if (strcmp(command, "stop")==0)
                {
                    run = false;
                    break;
                }
            }
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            json_result.Accept(writer);

            spdlog::warn(buffer.GetString());
        } 
    } while (run);

    spdlog::info("binancenativeapp: MQTTClient_disconnect");
    MQTTClient_disconnect(client, 10000);
    spdlog::info("binancenativeapp: MQTTClient_destroy");
    MQTTClient_destroy(&client);

    spdlog::info("binancenativeapp: Bye bye");
    return 0;
}