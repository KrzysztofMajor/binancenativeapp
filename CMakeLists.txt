project(binancenativeapp)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

find_package(eclipse-paho-mqtt-c CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libmongoc-1.0 CONFIG REQUIRED)
find_package(libarchive REQUIRED)

file(GLOB SOURCE_CPP *.cpp)    
file(GLOB SOURCE_HEADER *.h)    

add_executable(${PROJECT_NAME} ${SOURCE_CPP} ${SOURCE_HEADER})

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog spdlog::spdlog_header_only)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PROJECT_NAME} PRIVATE cxxopts::cxxopts)
target_link_libraries(${PROJECT_NAME} PRIVATE websockets)    
target_link_libraries(${PROJECT_NAME} PRIVATE ${MOSQUITTO_LIBRARIES})    
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LibArchive_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${MONGOC_INCLUDE_DIRS})

message(${MONGOC_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${MONGOC_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${MONGOC_DEFINITIONS})

if(WIN32)
target_link_libraries(${PROJECT_NAME} PRIVATE eclipse-paho-mqtt-c::paho-mqtt3a eclipse-paho-mqtt-c::paho-mqtt3c )
else()
target_link_libraries(${PROJECT_NAME} PRIVATE eclipse-paho-mqtt-c::paho-mqtt3as-static eclipse-paho-mqtt-c::paho-mqtt3cs-static)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${LibArchive_INCLUDE_DIRS})