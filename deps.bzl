"""
    @author Franco Chen
    @desc Load external dependencies
    @date 2022/09/06
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _build_external_local_repo(path, build_file, repo = None):
    if not repo:
        repo = build_file
    if repo not in native.existing_rules():
        native.new_local_repository(
            name = repo,
            path = path,
            build_file = build_file,
        )

def external_build(name):
    """
    BUILD external dependencies

    Args:
        name: Workspace name
    """

    if "io_bazel_rules_docker" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_docker",
            strip_prefix = "rules_docker-0.16.0-beta.2",
            urls = ["https://github.com/francoccc/rules_docker/archive/refs/tags/v0.16.0-beta.2.tar.gz"],
        )

    if "com_github_grpc_grpc" not in native.existing_rules():
        # http_archive(
        #     name = "com_github_grpc_grpc",
        #     urls = [
        #         "https://github.com/grpc/grpc/archive/de893acb6aef88484a427e64b96727e4926fdcfd.tar.gz",
        #     ],
        #     strip_prefix = "grpc-de893acb6aef88484a427e64b96727e4926fdcfd",
        # )
        http_archive(
            name = "com_github_grpc_grpc",
            sha256 = "5c57ac2db69df584ddc4a7915a2f9e3bf7046af07a506efe4b412c5cebab1660",
            strip_prefix = "grpc-3ad945947c6d2f47186c9c082ac46c13958292a6",
            urls = ["https://github.com/grpc/grpc/archive/3ad945947c6d2f47186c9c082ac46c13958292a6.tar.gz"],
        )

    if "build_stack_rules_proto" not in native.existing_rules():
        # Branch: master
        # Commit: 7c95feba87ae269d09690fcebb18c77d8b8bcf6a
        # Date: 2021-11-16 02:17:58 +0000 UTC
        # URL: https://github.com/stackb/rules_proto/commit/7c95feba87ae269d09690fcebb18c77d8b8bcf6a
        #
        # V2 (#193)
        # Size: 885598 (886 kB)
        http_archive(
            name = "build_stack_rules_proto",
            sha256 = "1190c296a9f931343f70e58e5f6f9ee2331709be4e17001bb570e41237a6c497",
            strip_prefix = "rules_proto-7c95feba87ae269d09690fcebb18c77d8b8bcf6a",
            urls = ["https://github.com/stackb/rules_proto/archive/7c95feba87ae269d09690fcebb18c77d8b8bcf6a.tar.gz"],
        )

    if "build_bazel_rules_apple" not in native.existing_rules():
        http_archive(
            name = "build_bazel_rules_apple",
            sha256 = "0052d452af7742c8f3a4e0929763388a66403de363775db7e90adecb2ba4944b",
            urls = [
                "https://storage.googleapis.com/grpc-bazel-mirror/github.com/bazelbuild/rules_apple/releases/download/0.31.3/rules_apple.0.31.3.tar.gz",
                "https://github.com/bazelbuild/rules_apple/releases/download/0.31.3/rules_apple.0.31.3.tar.gz",
            ],
        )

    if "build_bazel_apple_support" not in native.existing_rules():
        http_archive(
            name = "build_bazel_apple_support",
            sha256 = "76df040ade90836ff5543888d64616e7ba6c3a7b33b916aa3a4b68f342d1b447",
            urls = [
                "https://storage.googleapis.com/grpc-bazel-mirror/github.com/bazelbuild/apple_support/releases/download/0.11.0/apple_support.0.11.0.tar.gz",
                "https://github.com/bazelbuild/apple_support/releases/download/0.11.0/apple_support.0.11.0.tar.gz",
            ],
        )

    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            sha256 = "5bb6b0253ccf64b53d6c7249625a7e3f6c3bc6402abd52d3778bfa48258703a0",
            strip_prefix = "googleapis-2f9af297c84c55c8b871ba4495e01ade42476c92",
            urls = [
                "https://storage.googleapis.com/grpc-bazel-mirror/github.com/googleapis/googleapis/archive/2f9af297c84c55c8b871ba4495e01ade42476c92.tar.gz",
                "https://github.com/googleapis/googleapis/archive/2f9af297c84c55c8b871ba4495e01ade42476c92.tar.gz",
            ],
        )

    if "onload" not in native.existing_rules():
        http_archive(
            name = "onload",
            build_file = "//bazel:openonload.BUILD",
            urls = ["http://headquarter.prod.highfortfunds.com/s3_api/dev/hippo/onload-7.1.3.202.tgz"],
            strip_prefix = "onload-7.1.3.202",
            sha256 = "1eaee3039bf8ad53f3879f37d6e3eb08017e8bc050ffc23c9d6ef4018b208b9e",
            patches = [
                "//driver/patches:driver_object.c.patch",
                "//driver/patches:vi.h.patch",
            ],
        )

    _build_external_local_repo("contrib/linux-headers-common", "//bazel:linux_headers.BUILD", "linux")
    # _build_external_local_repo("contrib/onload", "//bazel:openonload.BUILD", "onload")
