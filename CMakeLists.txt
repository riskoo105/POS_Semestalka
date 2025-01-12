cmake_minimum_required(VERSION 3.30)
project(SemestralnaPracaHad C)

set(CMAKE_C_STANDARD 11)

# Pre server
add_executable(server
        game_logic.h
        game_logic.c
        server.c)
target_link_libraries(server pthread)

# Pre klienta
add_executable(client
        game_logic.h
        game_logic.c
        client.c)
target_link_libraries(client pthread)

# Pridanie cieľa pre spustenie oboch procesov
add_custom_target(run
        COMMAND ./server &
        COMMAND ./client
        DEPENDS server client
)
