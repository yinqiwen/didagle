load("@rules_cc//cc:defs.bzl", "cc_test")

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

cc_library(
    name = "test_common",
    hdrs = ["test_common.h"],
    deps = [
        "//didagle",
    ],
)

cc_test(
    name = "test_di",
    size = "small",
    srcs = ["test_di.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        "//didagle/di:container",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_simple_task",
    size = "small",
    srcs = ["test_simple_task.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_simple_graph",
    size = "small",
    srcs = ["test_simple_graph.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_map_aggegate",
    size = "small",
    srcs = ["test_map_aggegate.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_graph_params",
    size = "small",
    srcs = ["test_graph_params.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_graph_expect_config",
    size = "small",
    srcs = ["test_graph_expect_config.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "//didagle/processor/impl",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_graph_match_params",
    size = "small",
    srcs = ["test_graph_match_params.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
        "//didagle/processor/impl",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "test_graph_bench",
    srcs = ["test_graph_bench.cpp"],
    linkopts = LINKOPTS,
    deps = [
        ":test_common",
        "@com_github_google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "test_dsl",
    srcs = [
        "test_dsl.cpp",
        "test_dsl_ops.cpp",
    ],
    linkopts = LINKOPTS,
    deps = [
        "//didagle",
    ],
)
