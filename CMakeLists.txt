cmake_minimum_required(VERSION 3.0)
project(enc)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

add_executable(deh main.c global.h global.c tpm.h tpm.c aes.h aes.c crypto.h crypto.c)
target_link_libraries(deh tspi crypto)

add_library(devicecrypto SHARED global.h global.c tpm.h tpm.c aes.h aes.c crypto.h crypto.c)
target_link_libraries(devicecrypto tspi crypto)
