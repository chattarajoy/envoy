#include "source/extensions/bootstrap/reverse_tunnel/reverse_connections_reporter.h"

#include "source/common/protobuf/protobuf.h"
#include "absl/strings/str_cat.h"
#include "source/common/grpc/common.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace ReverseConnection {

namespace {
constexpr uint32_t kDefaultRetryDelayMs = 1000;
}

ReverseConnectionsReporter::ReverseConnectionsReporter(
    const LocalInfo::LocalInfo& local_info, Upstream::ClusterManager& /*cluster_manager*/,
    Stats::Scope& /*scope*/, Grpc::RawAsyncClientPtr async_client, Event::Dispatcher& dispatcher,
    std::chrono::milliseconds initial_report_interval)
    : local_info_(local_info), async_client_(std::move(async_client)), dispatcher_(dispatcher),
      service_method_(*Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          "envoy.service.reverse_tunnel.v3."
          "ReverseConnectionsReportingService.StreamReverseConnections")),
      report_interval_(initial_report_interval) {
  retry_timer_ = dispatcher_.createTimer([this]() { establishNewStream(); });
  report_timer_ = dispatcher_.createTimer([this]() {
    flush();
    scheduleFlushTimer();
  });
}

void ReverseConnectionsReporter::start() { establishNewStream(); }

void ReverseConnectionsReporter::stop() {
  if (report_timer_) {
    report_timer_->disableTimer();
  }
  if (retry_timer_) {
    retry_timer_->disableTimer();
  }
  if (stream_ != nullptr) {
    stream_->resetStream();
    stream_ = nullptr;
  }
}

void ReverseConnectionsReporter::enqueueAdded(
    const envoy::service::reverse_tunnel::v3::ReverseConnectionInfo& info) {
  pending_added_.push_back(info);
}

void ReverseConnectionsReporter::enqueueRemoved(
    const envoy::service::reverse_tunnel::v3::ReverseConnectionInfo& info) {
  pending_removed_.push_back(info);
}

void ReverseConnectionsReporter::flush() {
  if (stream_ == nullptr) {
    return;
  }

  if (pending_added_.empty() && pending_removed_.empty()) {
    // Send heartbeat request with node and nonce to learn/refresh interval if needed
      request_.Clear();
  request_.mutable_node()->MergeFrom(local_info_.node());
  request_.set_nonce(nextNonce());
  auto buffer = Grpc::Common::serializeMessage(request_);
  stream_->sendMessageRaw(std::move(buffer), false);
    return;
  }

  request_.Clear();
  request_.mutable_node()->MergeFrom(local_info_.node());
  for (const auto& rc : pending_added_) {
    *request_.add_connections_info() = rc;
  }
  for (const auto& rc : pending_removed_) {
    *request_.add_removed_connections() = rc;
  }
  request_.set_nonce(nextNonce());

  auto buffer = Grpc::Common::serializeMessage(request_);
  stream_->sendMessageRaw(std::move(buffer), false);
  pending_added_.clear();
  pending_removed_.clear();
}

void ReverseConnectionsReporter::onCreateInitialMetadata(Http::RequestHeaderMap& /*metadata*/) {}

bool ReverseConnectionsReporter::onReceiveMessageRaw(Buffer::InstancePtr&& response) {
  envoy::service::reverse_tunnel::v3::StreamReverseConnectionsResponse message;
  if (!Grpc::Common::parseBufferInstance(std::move(response), message)) {
    return false;
  }
  if (message.has_report_interval()) {
    const auto ms = std::chrono::milliseconds(
        Protobuf::util::TimeUtil::DurationToMicroseconds(message.report_interval()) / 1000);
    if (ms.count() >= 0) {
      report_interval_ = ms;
      scheduleFlushTimer();
    }
  }
  return true;
}

void ReverseConnectionsReporter::onRemoteClose(Grpc::Status::GrpcStatus /*status*/,
                                               const std::string& /*message*/) {
  stream_ = nullptr;
  setRetryTimer();
}

void ReverseConnectionsReporter::establishNewStream() {
  if (stream_ != nullptr) {
    return;
  }

  stream_ = async_client_->startRaw(service_method_.service()->full_name(), service_method_.name(),
                                    *this, Http::AsyncClient::StreamOptions());
  if (stream_ == nullptr) {
    setRetryTimer();
    return;
  }

  // Send initial heartbeat to get server-configured interval
  scheduleFlushTimer();
  flush();
}

void ReverseConnectionsReporter::setRetryTimer() {
  if (retry_timer_) {
    retry_timer_->enableTimer(std::chrono::milliseconds(kDefaultRetryDelayMs));
  }
}

void ReverseConnectionsReporter::scheduleFlushTimer() {
  if (report_timer_ && report_interval_.count() > 0) {
    report_timer_->enableTimer(report_interval_);
  }
}

std::string ReverseConnectionsReporter::nextNonce() { return absl::StrCat(++nonce_counter_); }

} // namespace ReverseConnection
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy


