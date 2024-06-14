load("@rules_cc//cc:defs.bzl", "cc_test")

LINKOPTS = [
    "-L/usr/local/lib",
    "-L/usr/local/lib64",
    "-lfmt",
    "-lfolly",
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

cc_test(
    name = "test_di",
    size = "small",
    srcs = ["test_di.cpp"],
    linkopts = LINKOPTS,
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

cc_binary(
    name = "test_task",
    srcs = ["test_task.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle",
    ],
)
