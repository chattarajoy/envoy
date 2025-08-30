# RCRS (Reverse Connection Reporting Service) Test Guide

## 🎯 Quick Start

To test the RCRS feature in Envoy:

```bash
cd /home/joy.chattaraj/workspace/envoy/examples/reverse_connection_socket_interface

# Run the test setup (starts management server)
./test_rcrs_feature.sh

# In another terminal, build and run Envoy:
cd /home/joy.chattaraj/workspace/envoy
bazel build //source/exe:envoy-static
./bazel-bin/source/exe/envoy-static -c examples/reverse_connection_socket_interface/envoy-with-rcrs.yaml --log-level debug
```

## 📁 Essential Files

### 🖥️ Management Server
- **`rcrs_management_server.py`** - RCRS server that receives connection reports
  - Listens on port 9090
  - Logs all incoming RCRS requests  
  - Responds with ACK/NACK and configurable report intervals

### ⚙️ Envoy Configuration  
- **`envoy-with-rcrs.yaml`** - Envoy config with RCRS support
  - Configures reverse connection cluster
  - Points RCRS to localhost:9090
  - Sets up proper bootstrap extensions

### 🧪 Test Script
- **`test_rcrs_feature.sh`** - Interactive test runner
  - Starts management server
  - Provides Envoy build/run instructions
  - Shows expected metrics and behaviors

## 🔧 Configuration Notes

### RCRS Integration Status
The RCRS configuration in `envoy-with-rcrs.yaml` has the `rcrs_config` section **commented out**:

```yaml
bootstrap_extensions:
  - name: envoy.bootstrap.reverse_connection.upstream_reverse_connection_socket_interface
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.bootstrap.reverse_connection_socket_interface.v3.UpstreamReverseConnectionSocketInterface
      stat_prefix: "upstream_reverse_connection"
      # RCRS configuration - uncomment if available in your build
      # rcrs_config:
      #   grpc_service:
      #     envoy_grpc:
      #       cluster_name: rcrs_cluster
      #   initial_fetch_timeout: 10s
```

**To enable RCRS:**
1. Uncomment the `rcrs_config` section
2. Ensure your Envoy build includes RCRS support
3. The management server will receive connection reports

### Key Configuration Elements

1. **RCRS Cluster** - Points to management server:
   ```yaml
   - name: rcrs_cluster
     connect_timeout: 5s
     type: STRICT_DNS
     load_assignment:
       endpoints:
         - lb_endpoints:
           - endpoint:
               address:
                 socket_address:
                   address: 127.0.0.1
                   port_value: 9090
   ```

2. **Reverse Connection Cluster** - Triggers RCRS reports:
   ```yaml
   - name: reverse_connection_cluster
     lb_policy: CLUSTER_PROVIDED
     cluster_type:
       name: envoy.clusters.reverse_connection
   ```

## 📊 Expected Behavior

### Management Server Logs
When RCRS is working, you should see:
```
🚀 New RCRS stream from ipv4:127.0.0.1:54321
📋 RCRS Request #1
   Node ID: test-envoy-node  
   Cluster: test-cluster
   Nonce: 1234567890
   Added Connections: 1
     ✅ Added: on-prem-node at 1699123456.0
📤 Sending ACK response (interval=10s)
```

### Envoy Metrics
Check these RCRS-specific metrics:
```bash
curl http://localhost:9902/stats | grep reverse_connection_reporter
```

Expected metrics:
- `reverse_connection_reporter.requests` - Requests sent to RCRS server
- `reverse_connection_reporter.responses` - Responses received from RCRS server
- `reverse_connection_reporter.errors` - RCRS communication errors  
- `reverse_connection_reporter.retries` - Retry attempts
- `reverse_connection_reporter.connections_added` - Connections reported as added
- `reverse_connection_reporter.connections_removed` - Connections reported as removed

### Envoy Debug Logs
Look for these log messages:
- `"Establishing new gRPC bidi stream"` - RCRS stream creation
- `"Sending StreamReverseConnectionsRequest"` - RCRS requests
- `"New reverse connections response"` - RCRS responses
- `"Started report period with interval"` - Report interval configuration

## 🔍 Troubleshooting

### RCRS Server Not Receiving Connections
1. Check if `rcrs_config` is uncommented in Envoy config
2. Verify RCRS server is running on port 9090
3. Check Envoy logs for gRPC connection errors
4. Ensure RCRS support is compiled into Envoy build

### No Reverse Connection Activity
1. The test config creates a reverse connection cluster but doesn't simulate actual reverse tunnels
2. For full testing, you need both cloud and on-prem Envoy instances
3. Use existing reverse connection examples to generate actual connection events

### Proto Compilation Issues
The management server includes fallback mock mode:
```bash
python rcrs_management_server.py --mock --port 9090
```

## 🎛️ Implementation Details

### RCRS Protocol Flow
1. **Envoy** establishes gRPC stream to management server
2. **Envoy** sends initial request with node info and nonce
3. **Server** responds with report interval configuration  
4. **Envoy** monitors reverse connection write-ahead log
5. **Envoy** periodically reports connection changes
6. **Server** acknowledges each report with matching nonce

### Code Locations  
- **Implementation**: `source/extensions/clusters/reverse_connection/reverse_connection_reporter.cc:15-270`
- **Protocol**: `api/envoy/service/reverse_tunnel/v3/rcrs.proto`
- **Bootstrap Config**: `api/envoy/extensions/bootstrap/reverse_connection_socket_interface/v3/upstream_reverse_connection_socket_interface.proto`

## ✅ Success Criteria

RCRS is working correctly when you see:
1. ✅ Management server logs RCRS stream connections
2. ✅ Envoy metrics show non-zero `reverse_connection_reporter.*` values
3. ✅ Management server receives and ACKs connection reports
4. ✅ Report interval is respected (default 10s in test setup)
5. ✅ Connection state changes are properly tracked and reported

---

**Ready to test!** Run `./test_rcrs_feature.sh` to start the management server and follow the interactive prompts.