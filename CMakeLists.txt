cmake_minimum_required(VERSION 3.16)

project(BitrateSceneSwitch VERSION 1.0.8)

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
    src/servers/nms.cpp
    src/servers/nms.hpp
    src/servers/nimble.cpp
    src/servers/nimble.hpp
    src/servers/rist.cpp
    src/servers/rist.hpp
    src/servers/openirl.cpp
    src/servers/openirl.hpp
    src/servers/irlhosting.cpp
    src/servers/irlhosting.hpp
    src/servers/xiu.cpp
    src/servers/xiu.hpp
    src/chat-client.cpp
    src/chat-client.hpp
    src/ws-client.cpp
    src/ws-client.hpp
    src/kick-chat.cpp
    src/kick-chat.hpp
    src/twitch-pubsub.cpp
    src/twitch-pubsub.hpp
    src/update-checker.cpp
    src/update-checker.hpp
    src/websocket-vendor.cpp
    src/websocket-vendor.hpp
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
    target_link_libraries(${PROJECT_NAME} PRIVATE winhttp)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
    )
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.obsproject.bitrate-scene-switch"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-dead_strip_dylibs)
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
    )
endif()

# Install
if(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "obs-plugins/64bit"
        LIBRARY DESTINATION "obs-plugins/64bit"
    )
    install(DIRECTORY data/
        DESTINATION "data/obs-plugins/${PROJECT_NAME}"
    )
elseif(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION "."
        LIBRARY DESTINATION "."
    )
else()
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION "lib/obs-plugins"
    )
    install(DIRECTORY data/
        DESTINATION "share/obs/obs-plugins/${PROJECT_NAME}"
    )
endif()
