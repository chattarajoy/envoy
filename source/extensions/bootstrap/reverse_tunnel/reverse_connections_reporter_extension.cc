#include "source/extensions/bootstrap/reverse_tunnel/reverse_connections_reporter_extension.h"

#include "source/common/grpc/async_client_manager_impl.h"
#include "source/common/protobuf/utility.h"

#include "envoy/extensions/bootstrap/reverse_tunnel/v3/reverse_connections_reporter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace ReverseConnection {

ReverseConnectionsReporterExtension::ReverseConnectionsReporterExtension(
    Server::Configuration::ServerFactoryContext& context,
    const v3::ReverseConnectionsReporterConfig& proto_config)
    : context_(context), config_(proto_config) {}

void ReverseConnectionsReporterExtension::onServerInitialized() {
  // Create async client via cluster manager's AsyncClientManager
  auto& cm = context_.clusterManager();
  auto& local_info = context_.localInfo();
  auto& scope = context_.scope();

  auto async_client_or = cm.grpcAsyncClientManager().getOrCreateRawAsyncClient(
      config_.grpc_service(), scope, /*skip_cluster_check=*/false);
  if (!async_client_or.ok()) {
    ENVOY_LOG(error, "RCRS: failed to create async client: {}", async_client_or.status().message());
    return;
  }

  Grpc::RawAsyncClientPtr client;
  // Convert shared to unique for reporter ownership of the stream handle usage; clone a new uncached
  // client where exclusive lifetime is desired.
  // For simplicity, just create an uncached client here.
  auto factory_or = cm.grpcAsyncClientManager().factoryForGrpcService(config_.grpc_service(), scope,
                                                                      /*skip_cluster_check=*/false);
  if (!factory_or.ok()) {
    ENVOY_LOG(error, "RCRS: failed to create async client factory: {}", factory_or.status().message());
    return;
  }
  auto client_or = factory_or.value()->createUncachedRawAsyncClient();
  if (!client_or.ok()) {
    ENVOY_LOG(error, "RCRS: failed to create uncached async client: {}", client_or.status().message());
    return;
  }
  client = std::move(client_or.value());

  const auto initial_ms = std::chrono::milliseconds(
      Protobuf::util::TimeUtil::DurationToMilliseconds(config_.initial_report_interval()));

  reporter_ = std::make_unique<ReverseConnectionsReporter>(local_info, cm, scope, std::move(client),
                                                           context_.mainThreadDispatcher(), initial_ms);
  reporter_->start();

  // Inject reporter into acceptor extension if available
  auto* upstream_interface = Network::socketInterface(
      "envoy.bootstrap.reverse_connection.upstream_reverse_connection_socket_interface");
  if (upstream_interface) {
    auto* acceptor = static_cast<ReverseTunnelAcceptor*>(const_cast<Network::SocketInterface*>(upstream_interface));
    if (acceptor && acceptor->getExtension()) {
      acceptor->getExtension()->setReporter(reporter_.get());
      ENVOY_LOG(info, "RCRS: reporter registered with ReverseTunnelAcceptorExtension");
    }
  }
}

Server::BootstrapExtensionPtr ReverseConnectionsReporterFactory::createBootstrapExtensionTyped(
    const v3::ReverseConnectionsReporterConfig& proto_config,
    Server::Configuration::ServerFactoryContext& context) {
  return std::make_unique<ReverseConnectionsReporterExtension>(context, proto_config);
}

REGISTER_FACTORY(ReverseConnectionsReporterFactory, Server::Configuration::BootstrapExtensionFactory);

} // namespace ReverseConnection
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy


