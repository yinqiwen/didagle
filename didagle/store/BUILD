package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "background_worker",
    srcs = [
        "background_worker.cpp",
    ],
    hdrs = [
        "background_worker.h",
    ],
)

cc_library(
    name = "task",
    srcs = [
        "graph_task.cpp",
    ],
    hdrs = [
        "graph_task.h",
    ],
    deps = [
        "//didagle/processor",
    ],
)

cc_library(
    name = "store",
    srcs = [
        "cluster_context.cpp",
        "graph_context.cpp",
        "graph_store.cpp",
        "vertex_context.cpp",
    ],
    hdrs = [
        "cluster_context.h",
        "common.h",
        "graph_context.h",
        "graph_store.h",
        "vertex_context.h",
    ],
    deps = [
        ":background_worker",
        ":task",
        "//didagle/graph",
        "//didagle/log",
        "//didagle/processor",
    ],
)
