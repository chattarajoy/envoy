load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_extension(
    name = "reverse_connection_lib",
    srcs = [
        "reverse_connection.cc",
        "reverse_connection_reporter.cc",
    ],
    hdrs = [
        "reverse_connection.h", 
        "reverse_connection_reporter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//envoy/upstream:cluster_factory_interface",
        "//source/common/grpc:async_client_lib",
        "//source/common/grpc:typed_async_client_lib", 
        "//source/common/http:header_utility_lib",
        "//source/common/network:address_lib",
        "//source/common/protobuf:utility_lib",
        "//source/common/upstream:cluster_factory_lib",
        "//source/common/upstream:upstream_includes",
        "//source/extensions/bootstrap/reverse_tunnel:reverse_tunnel_acceptor_lib",
        "//source/extensions/transport_sockets/raw_buffer:config",
        "@envoy_api//envoy/config/cluster/v3:pkg_cc_proto",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto", 
        "@envoy_api//envoy/config/endpoint/v3:pkg_cc_proto",
        "@envoy_api//envoy/extensions/clusters/reverse_connection/v3:pkg_cc_proto",
        "@envoy_api//envoy/service/reverse_tunnel/v3:pkg_cc_proto",
    ],
)
