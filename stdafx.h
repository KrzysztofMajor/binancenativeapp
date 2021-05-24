#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <string_view>

#include <zlib.h>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include <curl/curl.h>
#include <libwebsockets.h>
#include <paho-mqtt/MQTTClient.h>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <mongoc/mongoc.h>

#include "concurrentqueue.h"
#include "utils.h"

