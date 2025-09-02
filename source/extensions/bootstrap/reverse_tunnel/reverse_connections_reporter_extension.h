#pragma once

#include "envoy/server/bootstrap_extension_config.h"

#include "envoy/local_info/local_info.h"
#include "envoy/stats/scope.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/common/logger.h"
#include "source/extensions/bootstrap/reverse_tunnel/reverse_connections_reporter.h"
#include "source/extensions/bootstrap/reverse_tunnel/reverse_tunnel_acceptor.h"
#include "source/extensions/bootstrap/reverse_tunnel/factory_base.h"

#include "envoy/extensions/bootstrap/reverse_tunnel/v3/reverse_connections_reporter.pb.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace ReverseConnection {

// Forward decl
namespace v3 = envoy::extensions::bootstrap::reverse_tunnel::v3;

class ReverseConnectionsReporterExtension : public Server::BootstrapExtension,
                                            public Logger::Loggable<Logger::Id::filter> {
public:
  ReverseConnectionsReporterExtension(Server::Configuration::ServerFactoryContext& context,
                                      const v3::ReverseConnectionsReporterConfig& proto_config);

  void onServerInitialized() override;
  void onWorkerThreadInitialized() override {}

private:
  Server::Configuration::ServerFactoryContext& context_;
  v3::ReverseConnectionsReporterConfig config_;
  std::unique_ptr<ReverseConnectionsReporter> reporter_;
};

class ReverseConnectionsReporterFactory
    : public ReverseConnectionBootstrapFactoryBase<v3::ReverseConnectionsReporterConfig,
                                                   ReverseConnectionsReporterExtension> {
public:
  ReverseConnectionsReporterFactory()
      : ReverseConnectionBootstrapFactoryBase<v3::ReverseConnectionsReporterConfig,
                                              ReverseConnectionsReporterExtension>(
            "envoy.bootstrap.reverse_tunnel.reverse_connections_reporter") {}

private:
  Server::BootstrapExtensionPtr createBootstrapExtensionTyped(
      const v3::ReverseConnectionsReporterConfig& proto_config,
      Server::Configuration::ServerFactoryContext& context) override;
};

} // namespace ReverseConnection
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy


