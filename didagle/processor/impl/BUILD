package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "impl",
    srcs = [
        "expr.cpp",
        "noop.cpp",
        "switch.cpp",
        "while.cpp",
    ],
    deps = [
        "//didagle/processor",
        "@ssexpr//ssexpr",
    ],
)
