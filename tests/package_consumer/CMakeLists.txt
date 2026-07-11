cmake_minimum_required(VERSION 3.20)

project(WhatsUIConsumerSmoke LANGUAGES CXX)

find_package(WhatsUI 0.1 CONFIG REQUIRED)

add_executable(whatsui_consumer_smoke main.cpp)
target_link_libraries(whatsui_consumer_smoke PRIVATE WhatsUI::WhatsUI)
target_compile_features(whatsui_consumer_smoke PRIVATE cxx_std_17)
