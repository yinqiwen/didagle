package(default_visibility = ["//visibility:public"])

LINKOPTS = [
    "-L/usr/local/lib",
    "-L/usr/local/lib64",
    "-lfolly",
    "-lfmt",
    "-lgflags",
    "-lglog",
    "-ldouble-conversion",
    "-liberty",
    "-levent",
    "-lunwind",
    "-lcrypto",
    "-lssl",
    "-ldl",
    "-lrt",
    "-lstdc++fs",
    "-lboost_context",
    "-lboost_filesystem",
]

load("@rules_proto//proto:defs.bzl", "proto_library")

cc_proto_library(
    name = "msg_cc_proto",
    deps = [":msg_proto"],
)

proto_library(
    name = "msg_proto",
    srcs = ["msg.proto"],
)

cc_library(
    name = "processor_lib",
    srcs = [
        "future_processor.cpp",
        "processor_0.cpp",
        "processor_1.cpp",
        "processor_2.cpp",
        "processor_3.cpp",
        "processor_4.cpp",
        "processor_5.cpp",
        "processor_6.cpp",
        "processor_expr.cpp",
        "recall_1.cpp",
        "recall_2.cpp",
        "recall_3.cpp",
        "recall_filter.cpp",
        "recall_merge.cpp",
    ],
    hdrs = ["expr.h"],
    deps = [
        ":msg_cc_proto",
        "//didagle:didagle_core",
        "@ssexpr",
    ],
    alwayslink = True,
)

cc_binary(
    name = "graph_processor_dump",
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        ":processor_lib",
        "//didagle:didagle_processor_dump",
        "//ispine:ispine_flags_impl",
    ],
)

cc_binary(
    name = "dot_graph",
    srcs = ["dot_example.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        ":processor_lib",
        "//didagle:didagle_core",
        "//ispine:ispine_flags_impl",
    ],
)

cc_binary(
    name = "run_dsl",
    srcs = ["run_dsl.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        ":processor_lib",
        "//didagle:didagle_core",
        "//ispine:ispine_flags_impl",
    ],
)

cc_library(
    name = "coro_processor_lib",
    srcs = [
        "coro_processor_0.cpp",
        "coro_processor_1.cpp",
        "coro_processor_2.cpp",
    ],
    hdrs = ["expr.h"],
    deps = [
        "//didagle:didagle_core",
        "@ssexpr",
    ],
    alwayslink = True,
)

cc_binary(
    name = "coro_run_dsl",
    srcs = ["coro_run_dsl.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        ":coro_processor_lib",
        "//ispine:ispine_flags_impl",
    ],
)
