cmake_minimum_required(VERSION 3.15)
project(account_transfer_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

set(COMMON_SOURCES
    src/account.c
    src/transfer.c
    src/csv_loader.c
    src/simulation.c
)

# Kjørbart simuleringsprogram
add_executable(bank_simulation src/main_simulation.c ${COMMON_SOURCES})

# Testprogram
add_executable(bank_tests tests/main_test_runner.c ${COMMON_SOURCES})

if (UNIX)
    target_link_libraries(bank_simulation m pthread)
    target_link_libraries(bank_tests m pthread)
endif()
