package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "dtype",
    hdrs = [
        "dtype.h",
    ],
)

cc_library(
    name = "container",
    srcs = [
        "container.cpp",
    ],
    hdrs = [
        "container.h",
    ],
    deps = [
        "//didagle/log",
        "@com_google_protobuf//:protobuf",
        "@kcfg",
    ],
)

cc_library(
    name = "reset",
    srcs = [
        "reset.cpp",
    ],
    hdrs = [
        "reset.h",
    ],
    deps = [
        "@com_google_protobuf//:protobuf",
    ],
)
