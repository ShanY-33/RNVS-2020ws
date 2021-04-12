cmake_minimum_required(VERSION 3.5.1)
project(Block8 C)

set(CMAKE_C_STANDARD 99)

add_executable(GoBackNReceiver GoBackNReceiver.c
        src/DataBuffer.c
        src/GoBackNMessageStruct.c
        src/SocketConnection.c
        src/CRC.c)
add_executable(GoBackNSender GoBackNSender.c
        src/DataBuffer.c
        src/GoBackNMessageStruct.c
        src/SocketConnection.c
        src/CRC.c)
target_include_directories(GoBackNReceiver PRIVATE include)
target_include_directories(GoBackNSender PRIVATE include)

