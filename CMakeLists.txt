cmake_minimum_required(VERSION 3.29)
project(voting_system C)

set(CMAKE_C_STANDARD 23)

add_executable(voting_system main.c sqlite3.c)
