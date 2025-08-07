#pragma once

#include "envoy/extensions/filters/http/reverse_conn_notifier/v3/reverse_conn_notifier.pb.h"
#include "envoy/http/filter.h"
#include "envoy/http/async_client.h"
#include "envoy/tracing/trace_driver.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ReverseConnNotifier {

/**
 * Per-listener immutable configuration for the filter.
 */
class ReverseConnNotifierFilterConfig {
public:
  explicit ReverseConnNotifierFilterConfig(
      const envoy::extensions::filters::http::reverse_conn_notifier::v3::ReverseConnNotifier& proto_config);

  const std::string& cluster() const { return cluster_; }
  const std::string& path() const { return path_; }
  const std::string& detectionHeader() const { return detection_header_; }

private:
  const std::string cluster_;
  const std::string path_;
  const std::string detection_header_;
};


using ReverseConnNotifierFilterConfigSharedPtr = std::shared_ptr<ReverseConnNotifierFilterConfig>;

/**
 * The actual stream decoder filter implementation. It inspects the incoming
 * request headers, decides whether it is a "reverse connection" request, and
 * issues asynchronous HTTP calls to a side-car service when the stream is
 * accepted and again when it terminates.
 */
class ReverseConnNotifierFilter : public Http::StreamEncoderFilter,
                                  public Http::AsyncClient::Callbacks,
                                  public Logger::Loggable<Logger::Id::filter> {
public:
  ReverseConnNotifierFilter(ReverseConnNotifierFilterConfigSharedPtr config,
                            Upstream::ClusterManager& cm);

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) override ;
  Http::Filter1xxHeadersStatus encode1xxHeaders(Http::ResponseHeaderMap&) override {
    // Currently, we do not handle 1xx headers in this filter.
    return Http::Filter1xxHeadersStatus::Continue;
  }
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override {
    return Http::FilterDataStatus::Continue;
  };
  Http::FilterTrailersStatus encodeTrailers(Http::ResponseTrailerMap&) override {
        return Http::FilterTrailersStatus::Continue;
  };
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap&) override {
    return Http::FilterMetadataStatus::Continue;
  }
  
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
    callbacks_ = &callbacks;
  };

  void onDestroy() override;

  // Http::AsyncClient::Callbacks (we ignore responses)
  void onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&&) override {}
  void onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) override {}
  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  bool isReverseConnection(const Http::RequestHeaderMap& headers) const;
  void notify(absl::string_view event, const Http::ResponseHeaderMap& headers);

  ReverseConnNotifierFilterConfigSharedPtr config_;
  Upstream::ClusterManager& cm_;
  Http::StreamEncoderFilterCallbacks* callbacks_{}; // not owned
  bool interested_{false};
};

} // namespace ReverseConnNotifier
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
