cmake_minimum_required(VERSION 3.21)

project(adn_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

add_executable(adn-viewer MACOSX_BUNDLE mainqt.cpp)
target_link_libraries(adn-viewer PRIVATE Qt6::Widgets)

set_target_properties(adn-viewer PROPERTIES
	MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)