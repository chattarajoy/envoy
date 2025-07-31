load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":reverse_conn_notifier_filter_lib",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/filters/http/common:factory_base_lib",
        "@com_envoyproxy_protoc_gen_validate//validate:cc_validate",
        "@envoy_api//envoy/extensions/filters/http/reverse_conn_notifier/v3:pkg_cc_proto",
    ],
)

envoy_cc_extension(
    name = "reverse_conn_notifier_filter_lib",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//envoy/http:filter_interface",
        "//envoy/server:filter_config_interface",
        "//envoy/tracing:trace_driver_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:logger_lib",
        "//source/common/http:codes_lib",
        "//source/common/http:headers_lib",
        "//source/common/http:utility_lib",
        # "@nlohmann_json//:nlohmann_json",
        "@envoy_api//envoy/extensions/filters/http/reverse_conn_notifier/v3:pkg_cc_proto",
    ],
)
