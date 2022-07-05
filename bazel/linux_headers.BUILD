load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")
package(default_visibility = ["//visibility:public"])

# print(glob([
#     "include/**/*.h",
#   ]))
  
cc_library(
  name = "headers",
  srcs = glob([
    "include/**/*.h",
  ]),
  hdrs = glob([
    "include/**/*.h",
  ])
)