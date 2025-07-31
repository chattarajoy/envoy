#include "source/extensions/filters/http/reverse_conn_notifier/config.h"

#include "envoy/registry/registry.h"
#include "envoy/extensions/filters/http/reverse_conn_notifier/v3/reverse_conn_notifier.pb.validate.h"

#include "source/common/protobuf/utility.h"
#include "source/extensions/filters/http/reverse_conn_notifier/filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ReverseConnNotifier {

Http::FilterFactoryCb ReverseConnNotifierFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::reverse_conn_notifier::v3::ReverseConnNotifier& proto_config,
    const std::string&, Server::Configuration::FactoryContext& context) {
  auto shared_config = std::make_shared<ReverseConnNotifierFilterConfig>(proto_config);

  return [shared_config, &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(
        std::make_shared<ReverseConnNotifierFilter>(shared_config, context.serverFactoryContext().clusterManager()));
  };
}

static Envoy::Registry::RegisterFactory<ReverseConnNotifierFilterConfigFactory,
                                        Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace ReverseConnNotifier
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
