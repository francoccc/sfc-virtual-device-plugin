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
      """

    if "io_bazel_rules_docker" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_docker",
            strip_prefix = "rules_docker-0.16.0-beta.2",
            urls = ["https://github.com/francoccc/rules_docker/archive/refs/tags/v0.16.0-beta.2.tar.gz"],
        )

    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            urls = [
                "https://github.com/grpc/grpc/archive/de893acb6aef88484a427e64b96727e4926fdcfd.tar.gz",
            ],
            strip_prefix = "grpc-de893acb6aef88484a427e64b96727e4926fdcfd",
        )

    _build_external_local_repo("contrib/linux-headers-common", "//bazel:linux_headers.BUILD", "linux")
    _build_external_local_repo("contrib/onload", "//bazel:openonload.BUILD", "onload")
