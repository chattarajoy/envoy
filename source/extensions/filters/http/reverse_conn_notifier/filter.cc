#include "source/extensions/filters/http/reverse_conn_notifier/filter.h"

#include "envoy/http/header_map.h"
#include "envoy/http/async_client.h"

#include "source/common/http/message_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ReverseConnNotifier {

ReverseConnNotifierFilterConfig::ReverseConnNotifierFilterConfig(
    const envoy::extensions::filters::http::reverse_conn_notifier::v3::ReverseConnNotifier& proto_config)
    : cluster_(proto_config.cluster()),
      path_(proto_config.path().empty() ? "/notify" : proto_config.path()),
      detection_header_(proto_config.detection_header().empty() ? "x-reverse-connect" : proto_config.detection_header()) {}

ReverseConnNotifierFilter::ReverseConnNotifierFilter(
    ReverseConnNotifierFilterConfigSharedPtr config, Upstream::ClusterManager& cm)
    : config_(std::move(config)), cm_(cm) {}

bool ReverseConnNotifierFilter::isReverseConnection(const Http::RequestHeaderMap& headers) const {
  return true;
  // Either CONNECT method or presence of detection header
  if (headers.getMethodValue() == Http::Headers::get().MethodValues.Connect) {
    return true;
  }
  const Http::LowerCaseString key(config_->detectionHeader());
  return !headers.get(key).empty();
}

Http::FilterHeadersStatus ReverseConnNotifierFilter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "ReverseConnNotifierFilter::encodeHeaders called with end_stream={}", end_stream);
  notify("accepted", headers);
  return Http::FilterHeadersStatus::Continue;
}

void ReverseConnNotifierFilter::onDestroy() {
  if (interested_ && callbacks_ && callbacks_->requestHeaders()) {
    // notify("terminated", *callbacks_->responseHeaders());
  }
  notify("terminated", *callbacks_->responseHeaders());
}

void ReverseConnNotifierFilter::notify(absl::string_view event, const Http::ResponseHeaderMap& headers) {
  // Log the notification event
  ENVOY_LOG(info, "Reverse connection notification: event={}, headers_count={}", event, headers.size());
  
  // Log all headers
  headers.iterate([](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    ENVOY_LOG(info, "Header: {}={}", header.key().getStringView(), header.value().getStringView());
    return Http::HeaderMap::Iterate::Continue;
  });
  
  // TODO: Implement actual HTTP notification to sidecar service
  // This would involve creating an HTTP request message and sending it to the configured cluster
  // using the cluster manager (cm_) that's currently unused but available for this purpose
  [[maybe_unused]] auto& unused_cm = cm_;
}

} // namespace ReverseConnNotifier
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
