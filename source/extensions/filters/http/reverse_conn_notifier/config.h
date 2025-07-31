#pragma once

#include "envoy/extensions/filters/http/reverse_conn_notifier/v3/reverse_conn_notifier.pb.h"

#include "source/extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ReverseConnNotifier {

class ReverseConnNotifierFilterConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::http::reverse_conn_notifier::v3::ReverseConnNotifier> {
public:
  ReverseConnNotifierFilterConfigFactory() : FactoryBase("reverse_conn_notifier") {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::reverse_conn_notifier::v3::ReverseConnNotifier& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;
};

} // namespace ReverseConnNotifier
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
