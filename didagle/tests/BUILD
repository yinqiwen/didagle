load("@rules_cc//cc:defs.bzl", "cc_test")

LINKOPTS = [
    "-L/usr/local/lib",
    "-L/usr/local/lib64",
    "-lfolly",
    "-lfmt",
    # "-lgflags",
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
    name = "test_proc",
    size = "small",
    srcs = ["test_processor.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle/processor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_if_else",
    size = "small",
    srcs = ["test_if_else.cpp"],
    linkopts = LINKOPTS,
    linkstatic = True,
    deps = [
        ":test_common",
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

cc_binary(
    name = "test_proc_bench",
    srcs = ["test_proc_bench.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle/processor",
        "@com_github_google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "test_dsl",
    srcs = [
        "test_dsl.cpp",
        "test_dsl_processors.cpp",
    ],
    linkopts = LINKOPTS,
    deps = [
        "//didagle/store",
    ],
)
