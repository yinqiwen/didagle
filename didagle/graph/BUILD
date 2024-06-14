package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "params",
    srcs = [
        "params.cpp",
    ],
    hdrs = [
        "params.h",
    ],
    deps = [
        "@kcfg",
    ],
)

cc_library(
    name = "graph",
    srcs = [
        "graph.cpp",
        "vertex.cpp",
    ],
    hdrs = [
        "graph.h",
        "vertex.h",
    ],
    deps = [
        ":params",
        "//didagle/log",
        "//didagle/processor",
    ],
)
