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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent Network Test LinguistTools)

include_directories(thirdparty)

# ---------------------------------------------------------------------------
# libckan: C++ 重写的 CKAN（静态库）
# ---------------------------------------------------------------------------
set(CKAN_SOURCES
    src/ckan/version.cpp
    src/ckan/relationship.cpp
    src/ckan/ckanmodule.cpp
    src/ckan/moduleinstalldescriptor.cpp
    src/ckan/installedmodule.cpp
    src/ckan/registry.cpp
    src/ckan/repository.cpp
    src/ckan/gameinstance.cpp
    src/ckan/downloader.cpp
    src/ckan/repoindex.cpp
    src/ckan/relationshipresolver.cpp
    src/ckan/moduleinstaller.cpp
    src/ckan/ckan.cpp
    thirdparty/miniz.c
)

set(CKAN_HEADERS
    src/ckan/ckan_export.h
    src/ckan/version.h
    src/ckan/relationship.h
    src/ckan/ckanmodule.h
    src/ckan/moduleinstalldescriptor.h
    src/ckan/installedmodule.h
    src/ckan/registry.h
    src/ckan/repository.h
    src/ckan/gameinstance.h
    src/ckan/downloader.h
    src/ckan/repoindex.h
    src/ckan/relationshipresolver.h
    src/ckan/moduleinstaller.h
    src/ckan/ckan.h
    thirdparty/miniz.h
)

add_library(libckan STATIC
    ${CKAN_SOURCES}
    ${CKAN_HEADERS}
)
target_include_directories(libckan PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ckan
    ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(libckan PUBLIC
    Qt6::Core
    Qt6::Network
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/configmanager.cpp
    src/instancemanager.cpp
    src/iconutils.cpp
    src/backgroundmanager.cpp
    src/ckanmanager.cpp
    src/pages/homepage.cpp
    src/pages/instancelistpage.cpp
    src/pages/instancedetailpage.cpp
    src/pages/modtablemodel.cpp
    src/pages/settingspage.cpp
    src/pages/saveslistpage.cpp
    src/pages/savedetailpage.cpp
    src/widgets/instanceitemwidget.cpp
    src/widgets/toggleswitch.cpp
)

set(HEADERS
    src/mainwindow.h
    src/configmanager.h
    src/instancemanager.h
    src/iconutils.h
    src/backgroundmanager.h
    src/ckanmanager.h
    src/pages/homepage.h
    src/pages/instancelistpage.h
    src/pages/instancedetailpage.h
    src/pages/modtablemodel.h
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
    libckan
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

# ---------------------------------------------------------------------------
# 单元测试：libckan 核心逻辑
# ---------------------------------------------------------------------------
enable_testing()

add_executable(test_libckan
    tests/test_libckan.cpp
)
target_include_directories(test_libckan PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ckan
    ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(test_libckan PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    libckan
)
add_test(NAME test_libckan COMMAND test_libckan)
