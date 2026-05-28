include(FetchContent)

# dylib - C++17 动态库加载库
FetchContent_Declare(
    dylib
    GIT_REPOSITORY https://github.com/martin-olivier/dylib
    GIT_TAG v2.2.1
)

# nlohmann/json - JSON 解析库
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(dylib json)
