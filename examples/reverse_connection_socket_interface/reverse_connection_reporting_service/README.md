# Reverse Connection Reporting Service (RCRS) Examples

This directory contains utilities and examples for working with Envoy's Reverse Connection Socket Interface and the Reverse Connection Reporting Service (RCRS).

## Overview

The Reverse Connection Reporting Service (RCRS) is used by Envoy to report reverse connection changes to a management server. This directory provides:

- 🔧 **Python gRPC bindings generation** with xds-protos integration
- 📊 **Management server implementation** for handling RCRS requests
- 🧪 **Testing utilities** and examples

## Generate Python gRPC Bindings with xds-protos Integration

### Quick Start

Simply run the generation script to automatically set up everything:

```bash
python3 examples/reverse_connection_socket_interface/reverse_connection_reporting_service/generate_rcrs_grpc.py
```

The script handles all dependencies and prerequisites automatically!

### What the Script Does

The generation script automatically:
1. 📦 Installs `xds-protos` from PyPI
2. 🔧 Generates Python gRPC service stubs for the RCRS proto
3. 📍 Places them in the Python site-packages alongside xds-protos
4. 🐍 Creates proper Python package structure with `__init__.py` files

### What Gets Installed

**Packages installed:**
- `xds-protos` - Standard XDS protocol buffer definitions
- `grpcio-tools` - Python gRPC code generation tools (if needed)

**Generated files location:**
```
site-packages/
├── xds/                           # xds-protos package
└── envoy/                         # Envoy API bindings (new)
    └── service/
        └── reverse_tunnel/
            └── v3/
                ├── __init__.py
                ├── rcrs_pb2.py        # Protocol buffer message classes
                └── rcrs_pb2_grpc.py   # gRPC service stubs
```

### Usage in Your Code

After running the script, you can import the bindings directly:

```python
import grpc
from envoy.service.reverse_tunnel.v3 import rcrs_pb2
from envoy.service.reverse_tunnel.v3 import rcrs_pb2_grpc

# Create a gRPC channel
channel = grpc.insecure_channel('localhost:50051')

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

## Generated gRPC Classes

The `rcrs_pb2_grpc.py` file contains:

- **`ReverseConnectionsReportingServiceStub`** - Client stub for making gRPC calls
- **`ReverseConnectionsReportingServiceServicer`** - Base class for implementing the service server  
- **`ReverseConnectionsReportingService`** - Service registration utilities

### Key Method

- **`StreamReverseConnections`** - Bidirectional streaming RPC method for reporting reverse connection changes

## RCRS Management Server

This directory also includes a complete management server implementation that demonstrates how to:

- Handle bidirectional streaming connections from Envoy instances
- Process reverse connection reports
- Send configuration responses
- Manage multiple Envoy clients

See `rcrs_management_server.py` for the full implementation.

## Script Features

✅ **Automatic dependency management** - Installs required packages  
✅ **Permission handling** - Works with both system and user site-packages  
✅ **Error checking** - Comprehensive validation and helpful error messages  
✅ **Proper Python packaging** - Creates `__init__.py` files for correct imports  
✅ **Integration with xds-protos** - Places files alongside standard XDS protos  
✅ **Reusable** - Run anytime to regenerate bindings  

## Prerequisites

- Python 3.6+ with pip
- Access to Envoy workspace (for proto files)
- Internet connection (for PyPI package installation)

## Alternative: Manual Generation (Legacy)

If you prefer the manual approach or need files in the bazel output directory:

### Prerequisites

1. Install grpcio-tools:
   ```bash
   pip install grpcio-tools
   ```

2. Build the base proto files first:
   ```bash
   bazel build @envoy_api//envoy/service/reverse_tunnel/v3:pkg_py_proto
   ```

### Manual Generation

```bash
python3 -m grpc_tools.protoc \
  --proto_path=api \
  --proto_path=bazel-envoy/external/com_google_protobuf/src \
  --proto_path=bazel-envoy/external/com_google_googleapis \
  --proto_path=bazel-envoy/external/com_envoyproxy_protoc_gen_validate \
  --proto_path=bazel-envoy/external/com_github_cncf_xds \
  --proto_path=bazel-envoy/external/envoy_api \
  --python_out=bazel-bin/external/envoy_api \
  --grpc_python_out=bazel-bin/external/envoy_api \
  envoy/service/reverse_tunnel/v3/rcrs.proto
```

This places files in: `bazel-bin/external/envoy_api/envoy/service/reverse_tunnel/v3/`

### Legacy Usage

```python
import grpc
import sys
sys.path.append('bazel-bin/external/envoy_api')

from envoy.service.reverse_tunnel.v3 import rcrs_pb2
from envoy.service.reverse_tunnel.v3 import rcrs_pb2_grpc
```

## Notes

- **Recommended**: Use the automatic generation script for seamless integration
- The script automatically detects and handles permission issues
- Falls back to user site-packages if system site-packages is not writable
- Creates the complete `envoy.service.reverse_tunnel.v3` package structure
- Compatible with existing xds-protos installations
- Regenerating is safe - existing files are properly overwritten

## Troubleshooting

**Import errors?** Make sure you've run the generation script successfully and check that the files exist in your site-packages.

**Permission denied?** The script should handle this automatically, but you can try running with `--user` flag for pip installations.

**Missing proto dependencies?** Ensure you're running from the Envoy workspace root and that bazel external dependencies are available.

**Legacy bazel approach not working?** Switch to the automatic generation script which handles all dependencies automatically.