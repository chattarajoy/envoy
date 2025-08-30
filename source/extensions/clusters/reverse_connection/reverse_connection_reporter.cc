#include "source/extensions/clusters/reverse_connection/reverse_connection_reporter.h"

#include <random>

#include "envoy/service/reverse_tunnel/v3/rcrs.pb.h"
#include "envoy/service/reverse_tunnel/v3/reverse_tunnel_handshake.pb.h"
#include "envoy/stats/scope.h"

#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace ReverseConnection {

ReverseConnectionReporter::ReverseConnectionReporter(
    const LocalInfo::LocalInfo& local_info,
    RevConCluster& reverse_connection_cluster,
    Stats::Scope& scope,
    Grpc::RawAsyncClientPtr async_client,
    Event::Dispatcher& dispatcher)
    : reverse_connection_cluster_(reverse_connection_cluster),
      stats_{ALL_REVERSE_CONNECTION_REPORTER_STATS(POOL_COUNTER_PREFIX(scope, "reverse_connection_reporter."))},
      async_client_(std::move(async_client)),
      service_method_(*Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          "envoy.service.reverse_tunnel.v3.ReverseConnectionsReportingService.StreamReverseConnections")),
      time_source_(dispatcher.timeSource()),
      rng_(time_source_.monotonicTime().time_since_epoch().count()),
      initialized_(false) {
  
  request_.mutable_node()->MergeFrom(local_info.node());
  
  retry_timer_ = dispatcher.createTimer([this]() -> void {
    stats_.retries_.inc();
    establishNewStream();
  });
  
  response_timer_ = dispatcher.createTimer([this]() -> void { 
    processWriteAheadLog(); 
    sendReverseConnectionsRequest(); 
  });
  
  generateNonce();
  establishNewStream();
}

ReverseConnectionReporter::~ReverseConnectionReporter() {
  if (retry_timer_) {
    retry_timer_->disableTimer();
  }
  if (response_timer_) {
    response_timer_->disableTimer();
  }
}

void ReverseConnectionReporter::generateNonce() {
  // Generate a random nonce for request correlation
  std::uniform_int_distribution<uint64_t> dist;
  current_nonce_ = std::to_string(dist(rng_));
}

void ReverseConnectionReporter::setRetryTimer() {
  ENVOY_LOG(info, "Reverse connection reporter stream/connection will retry in {} ms.", RETRY_DELAY_MS);
  retry_timer_->enableTimer(std::chrono::milliseconds(RETRY_DELAY_MS));
}

void ReverseConnectionReporter::establishNewStream() {
  ENVOY_LOG(debug, "Establishing new gRPC bidi stream for {}", service_method_.DebugString());
  stream_ = async_client_->start(service_method_, *this, Http::AsyncClient::StreamOptions());
  if (stream_ == nullptr) {
    ENVOY_LOG(warn, "Unable to establish new stream");
    handleFailure();
    return;
  }
  
  // Send initial request to get configuration
  request_.clear_connections_info();
  request_.clear_removed_connections();
  request_.set_listener_draining(false);
  request_.set_nonce(current_nonce_);
  
  sendReverseConnectionsRequest();
}

envoy::service::reverse_tunnel::v3::ReverseConnectionInfo 
ReverseConnectionReporter::createReverseConnectionInfo(
    const RevConCluster::HostMapping::WriteAheadLogEntry& log_entry) {
  
  envoy::service::reverse_tunnel::v3::ReverseConnectionInfo connection_info;
  
  // Create TunnelInitiatorIdentity from node_id
  auto* connection_id = connection_info.mutable_connection_identifier();
  connection_id->set_node_id(log_entry.node_id);
  
  // Set timestamp
  auto* timestamp = connection_info.mutable_timestamp();
  auto time_since_epoch = log_entry.timestamp.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch);
  auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_epoch - seconds);
  
  timestamp->set_seconds(seconds.count());
  timestamp->set_nanos(static_cast<int32_t>(nanos.count()));
  
  return connection_info;
}

void ReverseConnectionReporter::processWriteAheadLog() {
  std::vector<envoy::service::reverse_tunnel::v3::ReverseConnectionInfo> added_connections;
  std::vector<envoy::service::reverse_tunnel::v3::ReverseConnectionInfo> removed_connections;
  
  // Process all entries in the write-ahead log
  while (reverse_connection_cluster_.host_mapping_.hasLogEntries()) {
    auto log_entry_opt = reverse_connection_cluster_.host_mapping_.popLogEntry();
    if (!log_entry_opt.has_value()) {
      break;
    }
    
    const auto& log_entry = log_entry_opt.value();
    auto connection_info = createReverseConnectionInfo(log_entry);
    
    switch (log_entry.operation) {
      case RevConCluster::HostMapping::OperationType::INSERT:
        added_connections.push_back(connection_info);
        connection_state_[log_entry.node_id] = connection_info;
        stats_.connections_added_.inc();
        ENVOY_LOG(debug, "Processing INSERT for connection: {}", log_entry.node_id);
        break;
        
      case RevConCluster::HostMapping::OperationType::UPDATE:
        // For updates, we treat as added connections if new, or just update the state
        if (connection_state_.find(log_entry.node_id) == connection_state_.end()) {
          added_connections.push_back(connection_info);
          stats_.connections_added_.inc();
        }
        connection_state_[log_entry.node_id] = connection_info;
        ENVOY_LOG(debug, "Processing UPDATE for connection: {}", log_entry.node_id);
        break;
        
      case RevConCluster::HostMapping::OperationType::REMOVE:
        removed_connections.push_back(connection_info);
        connection_state_.erase(log_entry.node_id);
        stats_.connections_removed_.inc();
        ENVOY_LOG(debug, "Processing REMOVE for connection: {}", log_entry.node_id);
        break;
    }
  }
  
  // Update request with the processed changes
  request_.clear_connections_info();
  request_.clear_removed_connections();
  
  for (const auto& connection : added_connections) {
    *request_.add_connections_info() = connection;
  }
  
  for (const auto& connection : removed_connections) {
    *request_.add_removed_connections() = connection;
  }
  
  ENVOY_LOG(debug, "Processed {} added connections, {} removed connections", 
            added_connections.size(), removed_connections.size());
}

void ReverseConnectionReporter::sendReverseConnectionsRequest() {
  // Process any pending write-ahead log entries
  processWriteAheadLog();
  
  // Set the current nonce
  request_.set_nonce(current_nonce_);
  
  ENVOY_LOG(trace, "Sending StreamReverseConnectionsRequest: {}", request_.DebugString());
  stream_->sendMessage(request_, false);
  stats_.responses_.inc();
  
  // Generate new nonce for next request
  generateNonce();
  
  // If we have received initial configuration, start report period
  if (message_ && initialized_) {
    startReportPeriod();
  }
}

void ReverseConnectionReporter::startReportPeriod() {
  if (!message_) {
    ENVOY_LOG(debug, "No response message received yet, cannot start report period");
    return;
  }
  
  // Handle special case for immediate reporting (report_interval = 0)
  if (message_->report_interval().seconds() == 0 && message_->report_interval().nanos() == 0) {
    ENVOY_LOG(debug, "Immediate reporting mode - sending current connection state");
    
    // Send current connection state immediately
    request_.clear_connections_info();
    request_.clear_removed_connections();
    
    for (const auto& [node_id, connection_info] : connection_state_) {
      *request_.add_connections_info() = connection_info;
    }
    
    sendReverseConnectionsRequest();
    return;
  }
  
  // Schedule next report based on configured interval
  uint64_t interval_ms = DurationUtil::durationToMilliseconds(message_->report_interval());
  response_timer_->enableTimer(std::chrono::milliseconds(interval_ms));
  
  ENVOY_LOG(debug, "Started report period with interval: {} ms", interval_ms);
}

void ReverseConnectionReporter::handleFailure() {
  stats_.errors_.inc();
  setRetryTimer();
}

void ReverseConnectionReporter::onCreateInitialMetadata(Http::RequestHeaderMap& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReverseConnectionReporter::onReceiveInitialMetadata(Http::ResponseHeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReverseConnectionReporter::onReceiveMessage(
    std::unique_ptr<envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse>&& message) {
  ENVOY_LOG(debug, "New reverse connections response: {}", message->DebugString());
  message_ = std::move(message);
  initialized_ = true;
  
  // Handle ACK/NACK response
  if (!message_->request_nonce().empty()) {
    if (message_->request_nonce() != current_nonce_) {
      ENVOY_LOG(warn, "Received response with mismatched nonce. Expected: {}, Got: {}", 
                current_nonce_, message_->request_nonce());
    }
    
    if (message_->has_error_detail()) {
      ENVOY_LOG(error, "Received NACK response: {}", message_->error_detail().message());
      stats_.errors_.inc();
      return;
    } else {
      ENVOY_LOG(debug, "Received ACK response for nonce: {}", message_->request_nonce());
    }
  }
  
  startReportPeriod();
  stats_.requests_.inc();
}

void ReverseConnectionReporter::onReceiveTrailingMetadata(Http::ResponseTrailerMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReverseConnectionReporter::onRemoteClose(Grpc::Status::GrpcStatus status, const std::string& message) {
  response_timer_->disableTimer();
  stream_ = nullptr;
  
  if (status != Grpc::Status::WellKnownGrpcStatus::Ok) {
    ENVOY_LOG(warn, "{} gRPC config stream closed: {}, {}", service_method_.name(), status, message);
    handleFailure();
  } else {
    ENVOY_LOG(debug, "{} gRPC config stream closed gracefully, {}", service_method_.name(), message);
    setRetryTimer();
  }
}

} // namespace ReverseConnection
} // namespace Extensions
} // namespace Envoy