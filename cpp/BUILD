package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "didagle_core",
    srcs = [
        "di_container.cpp",
        "di_reset.cpp",
        "didagle_background.cpp",
        "didagle_event.cpp",
        "didagle_log.cpp",
        "graph.cpp",
        "graph_data.cpp",
        "graph_executor.cpp",
        "graph_params.cpp",
        "graph_processor.cpp",
        "graph_processor_di.cpp",
        "graph_vertex.cpp",
    ],
    hdrs = [
        "di_container.h",
        "di_reset.h",
        "didagle_background.h",
        "didagle_event.h",
        "didagle_log.h",
        "graph.h",
        "graph_data.h",
        "graph_executor.h",
        "graph_params.h",
        "graph_processor.h",
        "graph_processor_api.h",
        "graph_processor_di.h",
        "graph_vertex.h",
    ],
    deps = [
        "//ispine:ispine_flags",
        "//ispine/coro:coroutine",
        "//ispine/pool/obj_pool:ispine_obj_pool",
        "@com_github_cameron314_concurrentqueue//:concurrentqueue",
        "@com_google_protobuf//:protobuf",
        "@kcfg",
        "@spdlog",
    ],
    alwayslink = True,
)

cc_library(
    name = "didagle_processor_dump",
    srcs = [
        "processor_dump.cpp",
    ],
    deps = [
        ":didagle_core",
    ],
)
