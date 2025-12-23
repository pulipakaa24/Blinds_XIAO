# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.* ${CMAKE_SOURCE_DIR}/include/*.cpp)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp-nimble-cpp esp_socketio_client)
