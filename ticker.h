#pragma once

namespace binance
{
    class ticker
    {
    public:
        ticker(MQTTClient& mosq, binance::api& api)
            : mosq_(mosq), api_(api)
        {

        }

        void subscribe(const std::vector<std::string>& symbols, const std::string &stream)
        {
            std::vector<std::string> sub{};
            std::for_each(std::begin(symbols), std::end(symbols), [&](auto& s)
                {
                    sub.push_back(fmt::format("{}@{}", crypto::utils::to_lower(s), stream));
                });
            auto channel = crypto::utils::combine(sub, "/");

            channel = fmt::format("/stream?streams={}", channel);
            api_.connect_endpoint(channel, [&](std::string_view& value)
                {
                    rapidjson::Document json_result{};
                    json_result.Parse(value.data());
                    
                    auto streamName = std::string(json_result["stream"].GetString());
                    auto& payload = json_result["data"];                    

                    std::replace(std::begin(streamName), std::end(streamName), '@', '/');
                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    payload.Accept(writer);
                    
                    const char* data = buffer.GetString();
                    MQTTClient_publish(mosq_, streamName.c_str(), strlen(data), data, 0, 0, nullptr);
                });
        }
    private:
        MQTTClient& mosq_;
        binance::api& api_;        
    };
}