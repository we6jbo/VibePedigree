cmake_minimum_required(VERSION 3.21)
project(VibePedigree VERSION 0.3.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)
qt_add_executable(VibePedigree main.cpp mainwindow.cpp mainwindow.h mainwindow.ui)
target_link_libraries(VibePedigree PRIVATE Qt6::Widgets Qt6::Sql)

install(TARGETS VibePedigree RUNTIME DESTINATION bin)

