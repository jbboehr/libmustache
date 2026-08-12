cmake_minimum_required(VERSION 3.15)
project(mustache_consumer LANGUAGES CXX)

find_package(mustache 0.5 CONFIG REQUIRED)

add_executable(mustache_consumer main.cpp)
target_link_libraries(mustache_consumer PRIVATE mustache::mustache)
