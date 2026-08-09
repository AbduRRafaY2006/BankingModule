cmake_minimum_required(VERSION 3.14)
project(BankingAdminSFML CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)

add_executable(banking_admin_sfml
    src/main.cpp
  src/Launcher.cpp
    src/FileManager.cpp
    src/Bank.cpp
    src/Widgets.cpp
    src/UI.cpp
    src/ATM.cpp
)

target_include_directories(banking_admin_sfml PRIVATE src)
target_link_libraries(banking_admin_sfml PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)

add_custom_command(TARGET banking_admin_sfml POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:banking_admin_sfml>/assets)