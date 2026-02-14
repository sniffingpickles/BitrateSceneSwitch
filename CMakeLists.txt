cmake_minimum_required(VERSION 3.16)

project(BitrateSceneSwitch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_compile_definitions(PLUGIN_VERSION="${PROJECT_VERSION}")

# Find OBS
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# Find CURL
find_package(CURL REQUIRED)

# Plugin sources
add_library(${PROJECT_NAME} MODULE
    src/plugin-main.cpp
    src/settings-dialog.cpp
    src/settings-dialog.hpp
    src/switcher.cpp
    src/switcher.hpp
    src/config.cpp
    src/config.hpp
    src/http-client.cpp
    src/http-client.hpp
    src/stream-server.cpp
    src/stream-server.hpp
    src/servers/belabox.cpp
    src/servers/belabox.hpp
    src/servers/nginx.cpp
    src/servers/nginx.hpp
    src/servers/sls.cpp
    src/servers/sls.hpp
    src/servers/mediamtx.cpp
    src/servers/mediamtx.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Widgets
    Qt6::Core
    CURL::libcurl
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
    )
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.noalbs.bitrate-scene-switch"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
    )
endif()

# Install
if(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "obs-plugins/64bit"
    )
    install(DIRECTORY data/
        DESTINATION "data/obs-plugins/${PROJECT_NAME}"
    )
elseif(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION "."
    )
else()
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION "lib/obs-plugins"
    )
    install(DIRECTORY data/
        DESTINATION "share/obs/obs-plugins/${PROJECT_NAME}"
    )
endif()
