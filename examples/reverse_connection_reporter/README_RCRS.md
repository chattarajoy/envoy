# Reverse Connections Reporting Service (RCRS) Extension

This example demonstrates how to use the ReverseConnectionsReporter bootstrap extension to report reverse connection lifecycle events to a management service.

## Overview

The RCRS extension automatically reports when reverse connections are added or removed from the UpstreamSocketManager. It sends these events via a gRPC bi-directional stream to a configured management service.

## Configuration

The `cloud-envoy.yaml` has been updated to include:

1. **RCRS Bootstrap Extension**: Automatically reports connection events
2. **RCRS Cluster**: Points to the management service
3. **Mock RCRS Service**: Simple nginx container for testing

### Bootstrap Extension Configuration

```yaml
bootstrap_extensions:
- name: envoy.bootstrap.reverse_connection.reverse_connections_reporter
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.bootstrap.reverse_tunnel.v3.ReverseConnectionsReporterConfig
    grpc_service:
      envoy_grpc:
        cluster_name: rcrs_cluster
    initial_report_interval: 30s
```

### RCRS Cluster Configuration

```yaml
clusters:
- name: rcrs_cluster
  connect_timeout: 5s
  type: STRICT_DNS
  lb_policy: ROUND_ROBIN
  load_assignment:
    cluster_name: rcrs_cluster
    endpoints:
    - lb_endpoints:
      - endpoint:
          address:
            socket_address:
              address: rcrs-service
              port_value: 9090
  http2_protocol_options: {}
```

## Running the Example

1. **Start the services**:
   ```bash
   docker-compose up -d
   ```

2. **Test the RCRS extension**:
   ```bash
   python3 test_rcrs_extension.py
   ```

3. **Check Envoy logs** for RCRS activity:
   ```bash
   docker-compose logs cloud-envoy | grep -i rcrs
   ```

## Expected Behavior

When reverse connections are established or removed, you should see log messages like:

```
[info] ReverseConnectionsReporterExtension: Registered reporter with acceptor extension.
[debug] ReverseConnectionsReporter: Establishing new gRPC bidi stream
[debug] UpstreamSocketManager: Notify reporter of connection added/removed
```

## Testing Connection Events

To trigger connection events and see the RCRS in action:

1. **Start the reverse connection test**:
   ```bash
   python3 test_reverse_connections.py
   ```

2. **Monitor RCRS logs**:
   ```bash
   docker-compose logs -f cloud-envoy | grep -E "(rcrs|reporter|connection)"
   ```

## Customization

### Change RCRS Service

To use a different RCRS service, update the cluster configuration:

```yaml
clusters:
- name: rcrs_cluster
  # ... other config ...
  load_assignment:
    cluster_name: rcrs_cluster
    endpoints:
    - lb_endpoints:
      - endpoint:
          address:
            socket_address:
              address: your-rcrs-service.com
              port_value: 9090
```

### Adjust Reporting Interval

Modify the `initial_report_interval` in the bootstrap extension:

```yaml
bootstrap_extensions:
- name: envoy.bootstrap.reverse_connection.reverse_connections_reporter
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.bootstrap.reverse_tunnel.v3.ReverseConnectionsReporterConfig
    grpc_service:
      envoy_grpc:
        cluster_name: rcrs_cluster
    initial_report_interval: 60s  # Report every 60 seconds
```

## Troubleshooting

### Extension Not Loading

Check that the extension is properly registered:
```bash
curl http://localhost:8889/config_dump | jq '.configs[] | select(.["@type"] == "type.googleapis.com/envoy.admin.v3.BootstrapConfigDump") | .bootstrap.bootstrap_extensions'
```

### Connection Issues

If the RCRS service is unreachable:
1. Check that `rcrs-service` is running: `docker-compose ps`
2. Verify network connectivity: `docker-compose exec cloud-envoy ping rcrs-service`
3. Check cluster health: `curl http://localhost:8889/clusters | grep rcrs`

### Logs

Enable debug logging to see detailed RCRS activity:
```bash
docker-compose exec cloud-envoy envoy -c /etc/cloud-envoy.yaml --log-level debug
```
