cmake_minimum_required(VERSION 3.12)

project(csv-completeness-checker
    LANGUAGES CXX
    VERSION 0.1.0
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)



# optimize the release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()



# fetch contents
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        12.0.0
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    fast_cpp_csv_parser
    GIT_REPOSITORY https://github.com/ben-strasser/fast-cpp-csv-parser.git
)
FetchContent_MakeAvailable(fast_cpp_csv_parser)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.3.1
)
FetchContent_MakeAvailable(cxxopts)



# define PROJECT_SOURCES
file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp"
)


# configure the build
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE "PROJECT_VERSION=\"${PROJECT_VERSION}\"")

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sources"
    ${fast_cpp_csv_parser_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    fmt::fmt
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
)



# statically link the gcc and std libraries for linux
# if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#     target_link_options(${PROJECT_NAME} PRIVATE "-static-libgcc" "-static-libstdc++")
# endif()

