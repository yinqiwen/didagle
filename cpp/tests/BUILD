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

cc_test(
    name = "test_di",
    size = "small",
    srcs = ["test_di.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        "//didagle:didagle_core",
        "//ispine:ispine_flags_impl",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_proc",
    size = "small",
    srcs = ["test_processor.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        "//didagle:didagle_core",
        "//ispine:ispine_flags_impl",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "test_proc_bench",
    srcs = ["test_proc_bench.cpp"],
    linkopts = LINKOPTS,
    tags = ["no-cache"],
    deps = [
        "//didagle:didagle_core",
        "//ispine:ispine_flags_impl",
        "@com_github_google_benchmark//:benchmark",
    ],
)
