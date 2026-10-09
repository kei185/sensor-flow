CMAKE_MINIMUM_REQUIRED(VERSION 4.3.4)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-Werror")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -g -O0")

project(sf LANGUAGES CXX)

file(
    GLOB SRC
    src/application.cpp
    src/manager.cpp
    src/core/*.cpp
    src/demo/*.cpp
    src/utility/*.cpp
    src/worker/*.cpp
)

file(
    GLOB_RECURSE HEADER
    src/*.hpp
)

find_package(Boost REQUIRED)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_library(sensor-flow SHARED)
    # 使うやつに要求標準を知らせる
    target_compile_features(sensor-flow PUBLIC cxx_std_23)
    target_link_libraries(sensor-flow PRIVATE Boost::boost)

    # CMAKE_INSTALL_* を使う
    include(GNUInstallDirs)
    target_sources(sensor-flow
        # ソースをいれる
        PRIVATE
            ${SRC}
        PUBLIC
        # ヘッダの集合を宣言する
            FILE_SET HEADERS
            # FILE_SET　の　BASE DIR
            BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
            # ヘッダの集合に入れるファイルを指定する
            FILES ${HEADER}

        )

    install(TARGETS sensor-flow
                EXPORT sensor-flow
                # オブジェクトファイル置き場所
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                # ヘッダーファイル置き場所
                FILE_SET HEADERS
                    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            )
else()
    add_executable(main  ${SRC} src/main.cpp)
    # add_executable(main src/main.cpp)
    target_include_directories(main PUBLIC src)
    target_link_libraries(main PRIVATE Boost::boost)
endif()
