cmake_minimum_required(VERSION 3.16)
project(HelloKSPLauncher VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Output to dist directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/dist)
# 共享库输出到同一目录：Windows DLL 走 RUNTIME，Unix .so/.dylib 走 LIBRARY，
# 统一落到 dist，确保启动器/测试与 libckan 动态库同目录可直接运行。
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/dist)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent Network Test LinguistTools)

include_directories(thirdparty)

# ---------------------------------------------------------------------------
# libckan: C++ 重写的 CKAN（共享库，编译为 dll/dylib/so）
# 全类导出（CKAN_API），启动器只依赖 CKan 门面 + CKanConfig 调用。
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
    src/ckan/filelock.cpp
    src/ckan/downloader.cpp
    src/ckan/repoindex.cpp
    src/ckan/relationshipresolver.cpp
    src/ckan/moduleinstaller.cpp
    src/ckan/txfilemanager.cpp
    src/ckan/ckan.cpp
    src/ckan/modpackio.cpp
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
    src/ckan/filelock.h
    src/ckan/downloader.h
    src/ckan/repoindex.h
    src/ckan/relationshipresolver.h
    src/ckan/moduleinstaller.h
    src/ckan/txfilemanager.h
    src/ckan/ckanconfig.h
    src/ckan/ckan.h
    src/ckan/modpackio.h
    thirdparty/miniz.h
)

add_library(libckan SHARED
    ${CKAN_SOURCES}
    ${CKAN_HEADERS}
)
target_compile_definitions(libckan PRIVATE
    CKAN_BUILD_SHARED
    CKAN_BUILDING_LIB
)
# 收窄公开 include：只暴露库自身头文件目录；src/ 与 thirdparty/ 仅内部使用。
target_include_directories(libckan PUBLIC
    ${CMAKE_SOURCE_DIR}/src/ckan
)
target_include_directories(libckan PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(libckan PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/configmanager.cpp
    src/instancemanager.cpp
    src/instancemanager_keymap.cpp
    src/instancemanager_settings.cpp
    src/instancemanager_saves.cpp
    src/instancemanager_backup.cpp
    src/instancemanager_modpack.cpp
    src/steamdiscovery.cpp
    src/iconutils.cpp
    src/backgroundmanager.cpp
    src/ckanmanager.cpp
    src/pages/homepage.cpp
    src/pages/instancelistpage.cpp
    src/pages/instancedetailpage.cpp
    src/pages/instancedetailpage_settings.cpp
    src/pages/instancedetailpage_mods.cpp
    src/pages/instancedetailpage_modpack.cpp
    src/pages/modtablemodel.cpp
    src/pages/settingspage.cpp
    src/pages/aboutpage.cpp
    src/pages/saveslistpage.cpp
    src/pages/savedetailpage.cpp
    src/widgets/instanceitemwidget.cpp
    src/widgets/toggleswitch.cpp
    # 实例导出需在启动器内直接生成 ZIP（miniz 编译进 DLL 但未导出，故在 exe 内单独编译一份）
    thirdparty/miniz.c
)

set(HEADERS
    src/mainwindow.h
    src/configmanager.h
    src/instancemanager.h
    src/instancemanager_keymap.h
    src/steamdiscovery.h
    src/iconutils.h
    src/backgroundmanager.h
    src/ckanmanager.h
    src/pages/homepage.h
    src/pages/instancelistpage.h
    src/pages/instancedetailpage.h
    src/pages/modtablemodel.h
    src/pages/settingspage.h
    src/pages/aboutpage.h
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
# 消费 libckan.dll 需 dllimport（CKAN_BUILD_SHARED）；声明自身 include 目录
target_compile_definitions(${PROJECT_NAME} PRIVATE CKAN_BUILD_SHARED)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ckan
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
    # 将 appicon.ico 嵌入 exe，作为资源管理器/任务栏固定的 exe 图标。
    # 注意：源码目录 "Hello KSP Launcher" 含空格，CMake 会把含空格的 include 目录
    # 直接传给 windres，导致其内部预处理器(gcc)按空格拆解参数而失败。
    # 因此这里用独立自定义命令编译 app.rc（内容仅一条 ICON，无需任何 include），
    # 不传入 -I 标志，产出 .o 再链接进 exe，从而规避该问题。
    set(APPICON_ABS "${CMAKE_SOURCE_DIR}/resources/appicon.ico")
    configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    set(APPICON_OBJ ${CMAKE_CURRENT_BINARY_DIR}/appicon.res.o)
    add_custom_command(
        OUTPUT ${APPICON_OBJ}
        COMMAND ${CMAKE_RC_COMPILER} -O coff ${CMAKE_CURRENT_BINARY_DIR}/app.rc ${APPICON_OBJ}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        VERBATIM
    )
    add_custom_target(app_icon ALL DEPENDS ${APPICON_OBJ})
    add_dependencies(${PROJECT_NAME} app_icon)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${APPICON_OBJ})
endif()

# ---------------------------------------------------------------------------
# 单元测试
# ---------------------------------------------------------------------------
enable_testing()

# libckan 核心逻辑测试（纯库测试，不依赖启动器源码）
add_executable(test_libckan
    tests/test_libckan.cpp
    thirdparty/miniz.c
)
target_compile_definitions(test_libckan PRIVATE CKAN_BUILD_SHARED)
target_include_directories(test_libckan PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ckan
)
target_link_libraries(test_libckan PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    libckan
)
add_test(NAME test_libckan COMMAND test_libckan)

# 启动器逻辑测试（Steam 库发现等，依赖 src/ 下启动器源码，不链接 libckan）
add_executable(test_launcher
    tests/test_launcher.cpp
    src/steamdiscovery.cpp
)
target_include_directories(test_launcher PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_launcher PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_launcher COMMAND test_launcher)
