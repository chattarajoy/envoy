#pragma once

#include "envoy/event/dispatcher.h"
#include "envoy/service/reverse_tunnel/v3/rcrs.pb.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "source/common/common/logger.h"
#include "source/common/grpc/async_client_impl.h"
#include "source/common/grpc/typed_async_client.h"
#include "source/extensions/clusters/reverse_connection/reverse_connection.h"

#include "absl/container/flat_hash_map.h"

#include <random>

namespace Envoy {
namespace Extensions {
namespace ReverseConnection {

/**
 * All reverse connection reporter stats. @see stats_macros.h
 */
#define ALL_REVERSE_CONNECTION_REPORTER_STATS(COUNTER)                                            \
  COUNTER(requests)                                                                               \
  COUNTER(responses)                                                                              \
  COUNTER(errors)                                                                                 \
  COUNTER(retries)                                                                                \
  COUNTER(connections_added)                                                                      \
  COUNTER(connections_removed)

/**
 * Struct definition for all reverse connection reporter stats. @see stats_macros.h
 */
struct ReverseConnectionReporterStats {
  ALL_REVERSE_CONNECTION_REPORTER_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * ReverseConnectionReporter implements the Reverse Connection Reporting Service (RCRS).
 * It monitors the writeahead log from RevConCluster::HostMapping and reports connection
 * changes via bidirectional gRPC stream to the management server.
 */
class ReverseConnectionReporter
    : public Grpc::AsyncStreamCallbacks<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse>,
      public Logger::Loggable<Logger::Id::connection> {
public:
  ReverseConnectionReporter(const LocalInfo::LocalInfo& local_info,
                          RevConCluster& reverse_connection_cluster,
                          Stats::Scope& scope,
                          Grpc::RawAsyncClientPtr async_client,
                          Event::Dispatcher& dispatcher);

  ~ReverseConnectionReporter();

  // Grpc::AsyncStreamCallbacks
  void onCreateInitialMetadata(Http::RequestHeaderMap& metadata) override;
  void onReceiveInitialMetadata(Http::ResponseHeaderMapPtr&& metadata) override;
  void onReceiveMessage(
      std::unique_ptr<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse>&& message) override;
  void onReceiveTrailingMetadata(Http::ResponseTrailerMapPtr&& metadata) override;
  void onRemoteClose(Grpc::Status::GrpcStatus status, const std::string& message) override;

  const ReverseConnectionReporterStats& getStats() const { return stats_; }

  // TODO(htuch): Make this configurable or some static.
  const uint32_t RETRY_DELAY_MS = 5000;

private:
  void setRetryTimer();
  void establishNewStream();
  void sendReverseConnectionsRequest();
  void handleFailure();
  void processWriteAheadLog();
  void startReportPeriod();
  void generateNonce();
  
  // Convert HostMapping log entry to ReverseConnectionInfo proto
  envoy::service::reverse_tunnel::v3::ReverseConnectionInfo 
  createReverseConnectionInfo(const RevConCluster::HostMapping::WriteAheadLogEntry& log_entry);

  RevConCluster& reverse_connection_cluster_;
  ReverseConnectionReporterStats stats_;
  Grpc::AsyncClient<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsRequest,
                   envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse>
      async_client_;
  Grpc::AsyncStream<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsRequest> stream_{};
  const Protobuf::MethodDescriptor& service_method_;
  Event::TimerPtr retry_timer_;
  Event::TimerPtr response_timer_;
  
  envoy::service::reverse_tunnel::v3::StreamReverseConnectionsRequest request_;
  std::unique_ptr<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse> message_;
  
  // In-memory connection state tracking
  absl::flat_hash_map<std::string, envoy::service::reverse_tunnel::v3::ReverseConnectionInfo> connection_state_;
  
  // Current request nonce for correlation
  std::string current_nonce_;
  
  // TimeSource reference (must come before rng_)
  TimeSource& time_source_;
  
  // Random generator for nonce generation  
  std::mt19937 rng_;
  
  // Flag to track if we've received initial configuration
  bool initialized_;
};

using ReverseConnectionReporterPtr = std::unique_ptr<ReverseConnectionReporter>;

} // namespace ReverseConnection
} // namespace Extensions
} // namespace Envoy