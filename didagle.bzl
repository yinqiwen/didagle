load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def clean_dep(dep):
    return str(Label(dep))

def didagle_workspace(path_prefix = "", tf_repo_name = "", **kwargs):
    http_archive(
        name = "bazel_skylib",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
        ],
        sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
    )
    http_archive(
        name = "rules_cc",
        sha256 = "35f2fb4ea0b3e61ad64a369de284e4fbbdcdba71836a5555abb5e194cf119509",
        strip_prefix = "rules_cc-624b5d59dfb45672d4239422fa1e3de1822ee110",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/624b5d59dfb45672d4239422fa1e3de1822ee110.tar.gz",
            "https://github.com/bazelbuild/rules_cc/archive/624b5d59dfb45672d4239422fa1e3de1822ee110.tar.gz",
        ],
    )

    # rules_proto defines abstract rules for building Protocol Buffers.
    http_archive(
        name = "rules_proto",
        sha256 = "2490dca4f249b8a9a3ab07bd1ba6eca085aaf8e45a734af92aad0c42d9dc7aaf",
        strip_prefix = "rules_proto-218ffa7dfa5408492dc86c01ee637614f8695c45",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/218ffa7dfa5408492dc86c01ee637614f8695c45.tar.gz",
            "https://github.com/bazelbuild/rules_proto/archive/218ffa7dfa5408492dc86c01ee637614f8695c45.tar.gz",
        ],
    )

    _TOML11_BUILD_FILE = """
cc_library(
    name = "toml",
    hdrs = glob([
        "**/*.hpp",
    ]),
    visibility = [ "//visibility:public" ],
)
"""
    toml11_ver = kwargs.get("toml11_ver", "3.6.1")
    toml11_name = "toml11-{ver}".format(ver = toml11_ver)
    http_archive(
        name = "com_github_toml11",
        strip_prefix = toml11_name,
        urls = [
            "https://mirrors.tencent.com/github.com/ToruNiina/toml11/archive/v{ver}.tar.gz".format(ver = toml11_ver),
            "https://github.com/ToruNiina/toml11/archive/v{ver}.tar.gz".format(ver = toml11_ver),
        ],
        build_file_content = _TOML11_BUILD_FILE,
    )

    _RAPIDJSON_BUILD_FILE = """
cc_library(
    name = "rapidjson",
    hdrs = glob(["include/rapidjson/**/*.h"]),
    includes = ["include"],
    defines = ["RAPIDJSON_HAS_STDSTRING=1"],
    visibility = [ "//visibility:public" ],
)
"""
    rapidjson_ver = kwargs.get("rapidjson_ver", "1.1.0")
    rapidjson_name = "rapidjson-{ver}".format(ver = rapidjson_ver)
    http_archive(
        name = "com_github_tencent_rapidjson",
        strip_prefix = rapidjson_name,
        urls = [
            "https://mirrors.tencent.com/github.com/Tencent/rapidjson/archive/v{ver}.tar.gz".format(ver = rapidjson_ver),
            "https://github.com/Tencent/rapidjson/archive/v{ver}.tar.gz".format(ver = rapidjson_ver),
        ],
        build_file_content = _RAPIDJSON_BUILD_FILE,
    )

    com_github_jbeder_yaml_cpp_ver = kwargs.get("com_github_jbeder_yaml_cpp_ver", "0.7.0")
    com_github_jbeder_yaml_cpp_urls = [
        "https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-{ver}.tar.gz".format(ver = com_github_jbeder_yaml_cpp_ver),
    ]
    http_archive(
        name = "com_github_jbeder_yaml_cpp",
        strip_prefix = "yaml-cpp-yaml-cpp-{ver}".format(ver = com_github_jbeder_yaml_cpp_ver),
        urls = com_github_jbeder_yaml_cpp_urls,
    )

    _SPDLOG_BUILD_FILE = """
cc_library(
    name = "spdlog",
    hdrs = glob([
        "include/**/*.h",
    ]),
    srcs= glob([
        "src/*.cpp",
    ]),
    defines = ["SPDLOG_FMT_EXTERNAL", "SPDLOG_COMPILED_LIB"],
    includes = ["include"],
    linkopts = ["-L/usr/local/lib64","-lfmt"],
    visibility = ["//visibility:public"],
)
"""
    spdlog_ver = kwargs.get("spdlog_ver", "1.14.1")
    spdlog_name = "spdlog-{ver}".format(ver = spdlog_ver)
    http_archive(
        name = "spdlog",
        strip_prefix = spdlog_name,
        urls = [
            "https://mirrors.tencent.com/github.com/gabime/spdlog/archive/v{ver}.tar.gz".format(ver = spdlog_ver),
            "https://github.com/gabime/spdlog/archive/v{ver}.tar.gz".format(ver = spdlog_ver),
        ],
        build_file_content = _SPDLOG_BUILD_FILE,
    )

    _XBYAK_BUILD_FILE = """
cc_library(
    name = "xbyak",
    hdrs = glob([
        "**/*.h",
    ]),
    includes=["./"],
    visibility = [ "//visibility:public" ],
)
"""
    xbyak_ver = kwargs.get("xbyak_ver", "5.992")
    xbyak_name = "xbyak-{ver}".format(ver = xbyak_ver)
    http_archive(
        name = "com_github_xbyak",
        strip_prefix = xbyak_name,
        urls = [
            "https://mirrors.tencent.com/github.com/herumi/xbyak/archive/v{ver}.tar.gz".format(ver = xbyak_ver),
            "https://github.com/herumi/xbyak/archive/v{ver}.tar.gz".format(ver = xbyak_ver),
        ],
        build_file_content = _XBYAK_BUILD_FILE,
    )

    fbs_ver = kwargs.get("fbs_ver", "2.0.0")
    fbs_name = "flatbuffers-{ver}".format(ver = fbs_ver)
    http_archive(
        name = "com_github_google_flatbuffers",
        strip_prefix = fbs_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/flatbuffers/archive/v{ver}.tar.gz".format(ver = fbs_ver),
            "https://github.com/google/flatbuffers/archive/v{ver}.tar.gz".format(ver = fbs_ver),
        ],
    )

    bench_ver = kwargs.get("bench_ver", "1.5.3")
    bench_name = "benchmark-{ver}".format(ver = bench_ver)
    http_archive(
        name = "com_github_google_benchmark",
        strip_prefix = bench_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
            "https://github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
        ],
    )

    protobuf_ver = kwargs.get("protobuf_ver", "3.19.2")
    protobuf_name = "protobuf-{ver}".format(ver = protobuf_ver)
    http_archive(
        name = "com_google_protobuf",
        strip_prefix = protobuf_name,
        urls = [
            "https://mirrors.tencent.com/github.com/protocolbuffers/protobuf/archive/v{ver}.tar.gz".format(ver = protobuf_ver),
            "https://github.com/protocolbuffers/protobuf/archive/v{ver}.tar.gz".format(ver = protobuf_ver),
        ],
    )

    gtest_ver = kwargs.get("gtest_ver", "1.14.0")
    gtest_name = "googletest-{ver}".format(ver = gtest_ver)
    http_archive(
        name = "com_google_googletest",
        strip_prefix = gtest_name,
        urls = [
            "https://github.com/google/googletest/archive/refs/tags/v{ver}.tar.gz".format(ver = gtest_ver),
        ],
    )

    git_repository(
        name = "kcfg",
        branch = "master",
        remote = "https://github.com/yinqiwen/kcfg.git",
    )

    git_repository(
        name = "ssexpr",
        branch = "main",
        remote = "https://github.com/yinqiwen/ssexpr.git",
    )
