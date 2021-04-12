project(Block5 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

string(APPEND CMAKE_C_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
string(APPEND CMAKE_EXE_LINKER_FLAGS "-fsanitize=address,undefined -static-libasan -static-libubsan")

add_executable(ntpclient client.c)

set_target_properties(ntpclient PROPERTIES OUTPUT_NAME "ntpclient")
