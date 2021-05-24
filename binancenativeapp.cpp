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

moodycamel::ConcurrentQueue<std::string> q(1024);

int mqtt_arrived_cb(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    /*std::string topic(topicName);

    std::string payload((char*)message->payload, message->payloadlen);    
    q.enqueue(payload);*/
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main(int argc, char** argv)
{        
    auto millisec_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    CurlGlobalStateGuard handle_curl_state{};    
    cxxopts::Options options("binancenativeapp", "binancenativeapp");
    options.add_options()
        ("h,host", "Multicast group", cxxopts::value<std::string>()->default_value("tcp://192.168.1.45:1883"))        
        ("m,mongo", "Mongo connection", cxxopts::value<std::string>()->default_value("mongodb://192.168.1.45:27017"))
        ("c,cert", "cert file", cxxopts::value<std::string>()->default_value("c:/project/client/binancenativeapp/cacert.pem"))
        ("s,symbols", "Multicast group", cxxopts::value<std::string>()->default_value("ethbtc;ltcbtc;bnbbtc"));

    auto result = options.parse(argc, argv);
    
    const char* host = result["host"].as<std::string>().c_str();
    const char* mongo = result["mongo"].as<std::string>().c_str();
    const char* cert = result["cert"].as<std::string>().c_str();    

    const auto symbols = result["symbols"].as<std::string>();    

    //binance::restful rest{};
    //rest.get_file("https://data.binance.vision/data/spot/monthly/klines/BTCUSDT/12h/BTCUSDT-12h-2021-04.zip");

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    auto client_id = crypto::utils::random_string(5);
    spdlog::info("Welcome to binancenativeapp: {}", client_id);
    MQTTClient_create(&client, host, client_id.c_str(),  MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    int rc = MQTTClient_setCallbacks(client, nullptr, connectionLost, mqtt_arrived_cb, delivered);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to set callback, return code {}", rc);
        return EXIT_FAILURE;
    }

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to connect, return code {}", rc);
        return EXIT_FAILURE;
    }    

    binance::api api{ cert };
    //api.get_exchange_info();

    const char* topic = "dogeusdt/#";
    int qos = 1;
    rc = MQTTClient_subscribe(client, topic, qos);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to subscrib for '{}', return code {}", topic, rc);
        return EXIT_FAILURE;
    }

    if (symbols.size())
    {
        auto pairs = crypto::utils::split(symbols, ';');
                        
        binance::ticker ticker_(client, api);
        
        ticker_.subscribe(pairs, "bookTicker");
        ticker_.subscribe(pairs, "kline_15m");
    }

    /*mongoc_init();

    bson_error_t error;
    mongoc_uri_t* uri = mongoc_uri_new_with_error(mongo, &error);
    auto* mongo_client = mongoc_client_new_from_uri(uri);
    if (!mongo_client)
    {
        spdlog::error("Failed to connect to mongodb");
        return EXIT_FAILURE;
    }
    mongoc_client_set_appname(mongo_client, client_id.c_str());
    auto database = mongoc_client_get_database(mongo_client, "db_name");
    auto collection = mongoc_client_get_collection(mongo_client, "db_name", "coll_name");
    auto insert = BCON_NEW("hello", BCON_UTF8("world"));
    if (!mongoc_collection_insert_one(collection, insert, NULL, NULL, &error)) 
    {
        fprintf(stderr, "%s\n", error.message);
    }
    bson_destroy(insert);
    //bson_destroy(&reply);
    //bson_destroy(command);
    //bson_free(str);

    
    mongoc_collection_destroy(collection);
    mongoc_database_destroy(database);
    mongoc_uri_destroy(uri);
    mongoc_client_destroy(mongo_client);
    mongoc_cleanup();*/

    std::thread consumer([&]() 
        {
            while (true)
            {
                std::string payload;
                if (q.try_dequeue(payload))
                {
                    rapidjson::Document json_result{};
                    json_result.Parse(payload.data());
                    
                    spdlog::info(payload);
                }
            }
        });
        
    auto start = std::chrono::steady_clock::now();
    while (1)    
        api.event_loop();    

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}