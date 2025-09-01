# 🔄 Reverse Connection Reporting Service (RCRS) Examples

This directory contains utilities and examples for working with Envoy's Reverse Connection Socket Interface and the Reverse Connection Reporting Service (RCRS).

## 🏗️ Architecture Overview

```
┌─────────────────┐    Reverse Connections    ┌─────────────────────┐    RCRS Reports    ┌─────────────────┐
│   On-Prem       │ ───────────────────────►  │   Cloud Envoy       │ ─────────────────► │  RCRS Server    │
│   Envoy         │                           │  (envoy-with-rcrs)  │                    │  (Management)   │
│                 │                           │                     │                    │                 │
│ Port: 8080      │                           │ Tunnel Port: 9000   │                    │ Port: 9090      │
│ Admin: 8879     │                           │ Admin: 9902         │                    │                 │
└─────────────────┘                           └─────────────────────┘                    └─────────────────┘
```

## 📁 Files Overview

### Configuration Files
- **`envoy-with-rcrs.yaml`** - Cloud-side Envoy with RCRS reporting enabled
- **`on-prem-envoy-with-rcrs.yaml`** - On-prem Envoy that initiates reverse connections

### Python Tools
- **`generate_rcrs_grpc.py`** - Generates Python gRPC bindings for RCRS
- **`rcrs_management_server.py`** - RCRS management server implementation
- **`test_rcrs_feature.sh`** - Interactive test script
- **`requirements.txt`** - Python dependencies

### Key Features
- ✅ **Proper RCRS Configuration**: Fixed bootstrap configuration per proto spec
- ✅ **Reverse Connection Initiation**: On-prem establishes connections to cloud
- ✅ **Connection Monitoring**: Cloud reports connection changes to RCRS server
- ✅ **Health Checking**: Ensures connectivity between components

## 🚀 Quick Start - Complete RCRS Testing

### Step 1: Generate Python Bindings (One-time setup)
```bash
cd /workspaces/envoy/examples/reverse_connection_socket_interface/reverse_connection_reporting_service

# Generate gRPC bindings automatically
python3 generate_rcrs_grpc.py
```

### Step 2: Start the RCRS Management Server
```bash
# Start the management server (listens on port 9090)
python3 rcrs_management_server.py
```

### Step 3: Build Envoy (if needed)
```bash
cd /workspaces/envoy

# Build Envoy with RCRS support
bazel build //source/exe:envoy-static
```

### Step 4: Start Cloud Envoy (RCRS-enabled)
```bash
# In a new terminal
cd /workspaces/envoy

# Start the cloud-side Envoy with RCRS reporting
./bazel-bin/source/exe/envoy-static \
  -c examples/reverse_connection_socket_interface/reverse_connection_reporting_service/envoy-with-rcrs.yaml \
  --log-level debug
```

### Step 5: Start On-Prem Envoy
```bash
# In another new terminal
cd /workspaces/envoy

# Start the on-prem Envoy that will initiate reverse connections
./bazel-bin/source/exe/envoy-static \
  -c examples/reverse_connection_socket_interface/reverse_connection_reporting_service/on-prem-envoy-with-rcrs.yaml \
  --log-level debug
```

### Step 6: Start Backend Service (Optional)
```bash
# Simple HTTP server for testing
python3 -m http.server 8081
```

## 🔍 What to Observe

### 1. RCRS Management Server Output
```
Starting RCRS Management Server on port 9090...
🚀 New RCRS stream from ipv4:127.0.0.1:54321
📋 RCRS Request #1
   Node ID: cloud-node
   Cluster: test-cluster
   Nonce: 1234567890
   Added Connections: 1
     ✅ Added: on-prem-rcrs-node at 1699123456.0
📤 Sending ACK response (interval=10s)
```

### 2. Cloud Envoy Logs (envoy-with-rcrs)
```
[info] Reverse Connection Reporting Service initialized successfully with new tracker-based implementation
[debug] ReverseConnectionTrackerManager: Initialized with thread-local slots
[debug] ReverseConnectionReporter: Establishing new gRPC bidi stream
[debug] ReverseConnectionTracker: Recording connection established - node: on-prem-rcrs-node
[debug] ReverseConnectionReporter: Processing 1 added, 0 removed events
[debug] ReverseConnectionReporter: Sending StreamReverseConnectionsRequest with 1 added, 0 removed
```

### 3. On-Prem Envoy Logs
```
[debug] Initiating reverse connection to cloud_rcrs
[info] Reverse connection established to 127.0.0.1:9000
[debug] gRPC handshake completed with cloud
```

### 4. Connection Metrics
Check the admin interfaces:

**Cloud Envoy Admin** (http://127.0.0.1:9902/stats):
```
reverse_connection_reporter.connections_added: 1
reverse_connection_reporter.connections_removed: 0
reverse_connection_reporter.requests: 5
reverse_connection_reporter.responses: 5
reverse_connection_reporter.events_processed: 3
reverse_connection_reporter.events_cleared: 3
reverse_connection_reporter.errors: 0
reverse_connection_reporter.retries: 0
```

**On-Prem Envoy Admin** (http://127.0.0.1:8879/stats):
```
downstream_reverse_connection.connections_established: 2
downstream_reverse_connection.handshakes_completed: 2
```

## 🧪 Testing the Full Flow

### Test 1: Basic Connectivity
```bash
# Send a request through the reverse connection
curl -H "x-remote-node-id: on-prem-rcrs-node" \
     -H "x-dst-cluster-uuid: on-prem-rcrs" \
     http://127.0.0.1:9000/test
```

### Test 2: Connection Lifecycle
1. Start all components as described above
2. Stop the on-prem Envoy
3. Observe RCRS server receiving "connection removed" reports
4. Restart on-prem Envoy
5. Observe RCRS server receiving "connection added" reports

## 📊 Key Configuration Differences

### Cloud Envoy (`envoy-with-rcrs.yaml`)
- **Bootstrap Extension**: `upstream_reverse_connection_socket_interface`
- **RCRS Config**: In `cluster_manager.reverse_connection_reporting_config`
- **Cluster Type**: `envoy.clusters.reverse_connection` (RevConCluster)
- **Listener**: Accepts reverse connections on port 9000

### On-Prem Envoy (`on-prem-envoy-with-rcrs.yaml`)
- **Bootstrap Extension**: `downstream_reverse_connection_socket_interface`
- **Connection Config**: Initiates connections to `cloud_rcrs` cluster
- **Node Identity**: `on-prem-rcrs-node` with cluster `on-prem-rcrs`
- **Frontend**: Accepts client requests on port 8080
- **Handshake Mode**: Uses legacy HTTP handshake (due to proto compatibility)

## 🔧 Python gRPC Bindings Generation

### Automatic Generation (Recommended)

Simply run the generation script to automatically set up everything:

```bash
python3 generate_rcrs_grpc.py
```

The script handles all dependencies and prerequisites automatically!

### What the Script Does

The generation script automatically:
1. 📦 Installs `xds-protos` from PyPI
2. 🔧 Generates Python gRPC service stubs for the RCRS proto
3. 📍 Places them in the Python site-packages alongside xds-protos
4. 🐍 Creates proper Python package structure with `__init__.py` files

### Usage in Your Code

After running the script, you can import the bindings directly:

```python
import grpc
from envoy.service.reverse_tunnel.v3 import rcrs_pb2
from envoy.service.reverse_tunnel.v3 import rcrs_pb2_grpc

# Create a gRPC channel
channel = grpc.insecure_channel('localhost:9090')

# Create a client stub
client = rcrs_pb2_grpc.ReverseConnectionsReportingServiceStub(channel)

# Create a request
request = rcrs_pb2.StreamReverseConnectionsRequest()
# ... configure your request ...

# Call the bidirectional streaming service
responses = client.StreamReverseConnections(iter([request]))
for response in responses:
    print(response)
```

## 🔧 Troubleshooting

### Issue: "No reverse connection cluster found"
**Solution**: Ensure the cloud Envoy has a cluster with `cluster_type.name: envoy.clusters.reverse_connection`

### Issue: "Unable to establish new stream"
**Solution**: 
1. Check RCRS server is running on port 9090
2. Verify `rcrs_cluster` configuration in cloud Envoy
3. Check network connectivity

### Issue: "Connection refused to cloud"
**Solution**:
1. Ensure cloud Envoy is running and listening on port 9000
2. Check `cloud_rcrs` cluster configuration in on-prem Envoy
3. Verify health checks are passing

### Issue: "gRPC handshake timeout" or "no such field: 'grpc_service'"
**Solution**:
1. If you get proto validation errors about `grpc_service` field, use legacy HTTP handshake:
   ```yaml
   # Replace grpc_service_config with:
   enable_legacy_http_handshake: true
   ```
2. For gRPC handshake issues:
   - Increase `handshake_timeout` in on-prem configuration
   - Check HTTP/2 protocol options are configured
   - Verify initial metadata headers

### Issue: RCRS Server Not Receiving Connections
1. Verify RCRS server is running on port 9090
2. Check Envoy logs for gRPC connection errors
3. Ensure RCRS support is compiled into Envoy build
4. Check `cluster_manager.reverse_connection_reporting_config` is properly configured

### Issue: Proto Compilation Issues
The management server includes fallback mock mode:
```bash
python rcrs_management_server.py --mock --port 9090
```

## 📈 Monitoring and Metrics

### RCRS Metrics (Cloud Envoy)
- `reverse_connection_reporter.connections_added`
- `reverse_connection_reporter.connections_removed`
- `reverse_connection_reporter.requests`
- `reverse_connection_reporter.responses`
- `reverse_connection_reporter.errors`
- `reverse_connection_reporter.retries`

### Reverse Connection Metrics (On-Prem Envoy)
- `downstream_reverse_connection.connections_established`
- `downstream_reverse_connection.handshakes_completed`
- `downstream_reverse_connection.connection_failures`

### Health Check Endpoints
- **Cloud Admin**: http://127.0.0.1:9902/stats
- **On-Prem Admin**: http://127.0.0.1:8879/stats
- **RCRS Server**: Check console output for connection reports

## 🎛️ Implementation Details

### RCRS Protocol Flow
1. **Envoy** establishes gRPC stream to management server
2. **Envoy** sends initial request with node info and nonce
3. **Server** responds with report interval configuration  
4. **UpstreamSocketManager** records socket events in thread-local tracker
5. **ReverseConnectionReporter** periodically collects events from tracker
6. **Envoy** sends connection changes to management server
7. **Server** acknowledges each report with matching nonce

### Code Locations  
- **New Implementation**: `source/extensions/clusters/reverse_connection/reverse_connection_reporter.cc`
- **Connection Tracker**: `source/extensions/clusters/reverse_connection/reverse_connection_tracker.cc`
- **Protocol**: `api/envoy/service/reverse_tunnel/v3/rcrs.proto`
- **Bootstrap Config**: `api/envoy/extensions/bootstrap/reverse_connection_socket_interface/v3/upstream_reverse_connection_socket_interface.proto`

## ✅ Success Criteria

🎯 **Complete RCRS functionality is working when you see:**

1. ✅ **RCRS Server receives connection reports**  
2. ✅ **On-prem establishes reverse connections**  
3. ✅ **Cloud Envoy reports to RCRS management server**  
4. ✅ **Metrics show successful connections and reports**  
5. ✅ **Connection lifecycle events are properly reported**
6. ✅ **Management server logs RCRS stream connections**
7. ✅ **Envoy metrics show non-zero `reverse_connection_reporter.*` values**
8. ✅ **Report interval is respected (default 10s)**

## 🧪 Alternative: Quick Test Script

For a guided testing experience, run:

```bash
./test_rcrs_feature.sh
```

This script provides interactive prompts and starts the management server automatically.

## Prerequisites

- Python 3.6+ with pip
- Access to Envoy workspace (for proto files)
- Internet connection (for PyPI package installation)
- Envoy built with RCRS support

---

**Ready to test the complete RCRS functionality!** This setup demonstrates the full RCRS workflow with proper configuration according to the Envoy bootstrap proto specification.