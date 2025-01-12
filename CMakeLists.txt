cmake_minimum_required(VERSION 3.20)
project(SemestralnaPracaHad C)

set(CMAKE_C_STANDARD 11)

# Zložky pre zdrojové súbory a hlavičky
set(CLIENT_DIR ${CMAKE_SOURCE_DIR}/Client)
set(SERVER_DIR ${CMAKE_SOURCE_DIR}/Server)
set(GAME_LOGIC_DIR ${CMAKE_SOURCE_DIR}/Game_logic)

# Pre server
add_executable(server
        ${GAME_LOGIC_DIR}/game_logic.c
        ${SERVER_DIR}/server.c
)
target_include_directories(server PRIVATE ${GAME_LOGIC_DIR})
target_link_libraries(server pthread)

# Pre klienta
add_executable(client
        ${GAME_LOGIC_DIR}/game_logic.c
        ${CLIENT_DIR}/client.c
)
target_include_directories(client PRIVATE ${GAME_LOGIC_DIR})
target_link_libraries(client pthread)

# Pridanie cieľa pre spustenie oboch procesov
add_custom_target(run
        COMMAND ./server &
        COMMAND sleep 1 && ./client
        DEPENDS server client
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
