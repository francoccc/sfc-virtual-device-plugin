load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

def _grpc_extra_deps():
    apple_rules_dependencies(ignore_version_differences = False)
    apple_support_dependencies()

    # Initialize Google APIs with only C++ and Python targets
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        grpc = True,
        python = True,
    )
