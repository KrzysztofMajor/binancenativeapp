project(binancenativeapp)
cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

find_package(mosquitto REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)


file(GLOB SOURCE_CPP *.cpp)    

add_executable(${PROJECT_NAME} ${SOURCE_CPP})

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog spdlog::spdlog_header_only)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PROJECT_NAME} PRIVATE cxxopts::cxxopts)
target_link_libraries(${PROJECT_NAME} PRIVATE websockets)    
target_link_libraries(${PROJECT_NAME} PRIVATE ${MOSQUITTO_LIBRARIES})

target_include_directories(${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${RAPIDJSON_INCLUDE_DIRS})

install(TARGETS ${PROJECT_NAME}
        CONFIGURATIONS Release
        RUNTIME DESTINATION bin)
		
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  DESTINATION bin
  FILES_MATCHING PATTERN "*.dll")