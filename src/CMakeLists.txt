# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp-nimble-cpp esp_socketio_client ulp)

# set(ulp_app_name "lp_core_${COMPONENT_NAME}")

# set(ulp_lp_core_sources "${CMAKE_SOURCE_DIR}/lp_core/main.c")
# set(ulp_exp_dep_srcs "${CMAKE_SOURCE_DIR}/src/lp_i2c.c")
# ulp_embed_binary(${ulp_app_name} "${ulp_lp_core_sources}" "${ulp_exp_dep_srcs}")