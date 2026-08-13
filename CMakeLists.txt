cmake_minimum_required(VERSION 3.16)
project(HelloKSPLauncher VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Output to dist directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/dist)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent LinguistTools)

include_directories(thirdparty)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/configmanager.cpp
    src/instancemanager.cpp
    src/iconutils.cpp
    src/backgroundmanager.cpp
    src/pages/homepage.cpp
    src/pages/instancelistpage.cpp
    src/pages/instancedetailpage.cpp
    src/pages/settingspage.cpp
    src/pages/saveslistpage.cpp
    src/pages/savedetailpage.cpp
    src/widgets/instanceitemwidget.cpp
    src/widgets/toggleswitch.cpp
    thirdparty/miniz.c
)

set(HEADERS
    src/mainwindow.h
    src/configmanager.h
    src/instancemanager.h
    src/iconutils.h
    src/backgroundmanager.h
    src/pages/homepage.h
    src/pages/instancelistpage.h
    src/pages/instancedetailpage.h
    src/pages/settingspage.h
    src/pages/saveslistpage.h
    src/pages/savedetailpage.h
    src/widgets/instanceitemwidget.h
    src/widgets/toggleswitch.h
    thirdparty/miniz.h
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Svg
    Qt6::Concurrent
)

# Copy translations (.qm files) to output directory
file(COPY ${CMAKE_SOURCE_DIR}/translations DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Custom target to regenerate .qm files from .ts files
add_custom_target(update_translations
    COMMAND Qt6::lrelease ${CMAKE_SOURCE_DIR}/translations/hello_ksp_launcher_zh_CN.ts
    COMMAND Qt6::lrelease ${CMAKE_SOURCE_DIR}/translations/hello_ksp_launcher_en_US.ts
    COMMENT "Regenerating .qm files from .ts files"
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
