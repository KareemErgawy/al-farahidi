cmake_minimum_required (VERSION 3.6)

set (PROJ_NAME al-farahidi)
project (${PROJ_NAME} VERSION 0.1.0)

include_directories (include)
set (SRCS src/main.c src/regex.c src/nfa.c)

add_executable (${PROJ_NAME} ${SRCS})
