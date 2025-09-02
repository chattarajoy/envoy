#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/config/core/v3/grpc_service.pb.h"
#include "envoy/grpc/async_client.h"
#include "envoy/local_info/local_info.h"
#include "envoy/service/reverse_tunnel/v3/rcrs.pb.h"
#include "envoy/stats/scope.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/common/logger.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace ReverseConnection {

/**
 * ReverseConnectionsReporter streams reverse connection add/remove events to an
 * external management server via the ReverseConnectionsReportingService (RCRS)
 * bi-directional gRPC stream.
 *
 * This reporter runs on the main thread and is fed from worker threads via
 * posted events (the posting mechanism is wired by the owning bootstrap
 * extension).
 */
class ReverseConnectionsReporter
    : public Grpc::RawAsyncStreamCallbacks,
      public Logger::Loggable<Logger::Id::filter> {
public:
  ReverseConnectionsReporter(const LocalInfo::LocalInfo& local_info,
                             Upstream::ClusterManager& cluster_manager, Stats::Scope& scope,
                             Grpc::RawAsyncClientPtr async_client, Event::Dispatcher& dispatcher,
                             std::chrono::milliseconds initial_report_interval);

  // Lifecycle
  void start();
  void stop();

  // Enqueue events (thread-safe via posting by owner; methods themselves are main-thread only)
  void enqueueAdded(const envoy::service::reverse_tunnel::v3::ReverseConnectionInfo& info);
  void enqueueRemoved(const envoy::service::reverse_tunnel::v3::ReverseConnectionInfo& info);
  void flush();

  // Grpc::RawAsyncStreamCallbacks
  void onCreateInitialMetadata(Http::RequestHeaderMap& metadata) override;
  void onReceiveInitialMetadata(Http::ResponseHeaderMapPtr&&) override {}
  bool onReceiveMessageRaw(Buffer::InstancePtr&& response) override;
  void onReceiveTrailingMetadata(Http::ResponseTrailerMapPtr&&) override {}
  void onRemoteClose(Grpc::Status::GrpcStatus status, const std::string& message) override;

private:
  void establishNewStream();
  void setRetryTimer();
  void scheduleFlushTimer();
  std::string nextNonce();

  const LocalInfo::LocalInfo& local_info_;
  Grpc::RawAsyncClientPtr async_client_;
  Event::Dispatcher& dispatcher_;
  const Protobuf::MethodDescriptor& service_method_;

  // Stream and timers
  Grpc::RawAsyncStream* stream_{nullptr};
  Event::TimerPtr retry_timer_;
  Event::TimerPtr report_timer_;

  // Request under construction and batching buffers
  envoy::service::reverse_tunnel::v3::StreamReverseConnectionsRequest request_;
  std::vector<envoy::service::reverse_tunnel::v3::ReverseConnectionInfo> pending_added_;
  std::vector<envoy::service::reverse_tunnel::v3::ReverseConnectionInfo> pending_removed_;

  // Reporting cadence and ack tracking
  std::chrono::milliseconds report_interval_;
  uint64_t nonce_counter_{0};
};

} // namespace ReverseConnection
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy


