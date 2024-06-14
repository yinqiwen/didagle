package(default_visibility = ["//visibility:public"])

exports_files(["graph_params.h"])

cc_library(
    name = "processor",
    srcs = [
        "processor.cpp",
        "processor_di.cpp",
    ],
    hdrs = [
        "api.h",
        "coroutine.h",
        "processor.h",
        "processor_di.h",
    ],
    deps = [
        "//didagle/di:container",
        "//didagle/di:reset",
        "//didagle/graph:params",
        "//didagle/log",
        "//didagle/trace:event",
        "@com_google_protobuf//:protobuf",
    ],
)
