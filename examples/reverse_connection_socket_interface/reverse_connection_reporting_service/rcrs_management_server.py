#!/usr/bin/env python3
"""
RCRS Management Server for testing Envoy's Reverse Connection Reporting Service.

This server implements the StreamReverseConnections gRPC service and can be used
to test the RCRS implementation in Envoy without requiring proto compilation.

Usage:
    python rcrs_management_server.py --port 9090 --report-interval 10
"""

import argparse
import asyncio
import json
import logging
import sys
import os
import time
from concurrent import futures
from datetime import datetime
from typing import Dict, List

# Add the generated proto path
proto_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../bazel-bin/external/envoy_api'))
if proto_path not in sys.path:
    sys.path.insert(0, proto_path)

import grpc
from grpc import aio

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class RCRSManagementServer:
    """
    Simple RCRS management server that handles StreamReverseConnections calls.
    
    This server logs all incoming connection reports and responds with ACKs
    and configurable report intervals.
    """
    
    def __init__(self, report_interval_seconds: int = 30):
        self.report_interval_seconds = report_interval_seconds
        self.connected_nodes: Dict[str, Dict] = {}
        self.request_count = 0
        
    async def stream_reverse_connections(self, request_iterator, context):
        """
        Handle the bidirectional StreamReverseConnections RPC.
        
        This method receives requests from Envoy and sends back responses
        with configuration and ACK/NACK status.
        """
        peer_info = context.peer()
        logger.info(f"🚀 New RCRS stream from {peer_info}")
        
        try:
            async for request in request_iterator:
                self.request_count += 1
                await self._process_request(request)
                response = await self._create_response(request)
                yield response
                
        except asyncio.CancelledError:
            logger.info(f"❌ RCRS stream cancelled by client {peer_info}")
        except Exception as e:
            logger.error(f"❌ Error in RCRS stream: {e}")
        finally:
            logger.info(f"✅ RCRS stream ended for {peer_info}")
    
    async def _process_request(self, request):
        """Process an incoming StreamReverseConnectionsRequest."""
        # Extract basic info
        node_id = getattr(request.node, 'id', 'unknown')
        cluster_name = getattr(request.node, 'cluster', 'unknown')
        nonce = getattr(request, 'nonce', '')
        listener_draining = getattr(request, 'listener_draining', False)
        
        logger.info("=" * 60)
        logger.info(f"📋 RCRS Request #{self.request_count}")
        logger.info(f"   Node ID: {node_id}")
        logger.info(f"   Cluster: {cluster_name}")
        logger.info(f"   Nonce: {nonce}")
        logger.info(f"   Draining: {listener_draining}")
        
        # Process connection additions
        connections_added = getattr(request, 'connections_info', [])
        logger.info(f"   Added Connections: {len(connections_added)}")
        
        for conn_info in connections_added:
            conn_id = getattr(conn_info.connection_identifier, 'node_id', 'unknown')
            timestamp = conn_info.timestamp
            
            logger.info(f"     ✅ Added: {conn_id} at {timestamp.seconds}.{timestamp.nanos}")
            
            self.connected_nodes[conn_id] = {
                'node_id': conn_id,
                'timestamp': datetime.fromtimestamp(timestamp.seconds),
                'status': 'connected',
                'reported_by': node_id
            }
        
        # Process connection removals
        connections_removed = getattr(request, 'removed_connections', [])
        logger.info(f"   Removed Connections: {len(connections_removed)}")
        
        for conn_info in connections_removed:
            conn_id = getattr(conn_info.connection_identifier, 'node_id', 'unknown')
            timestamp = conn_info.timestamp
            
            logger.info(f"     ❌ Removed: {conn_id} at {timestamp.seconds}.{timestamp.nanos}")
            
            if conn_id in self.connected_nodes:
                self.connected_nodes[conn_id]['status'] = 'disconnected'
                self.connected_nodes[conn_id]['timestamp'] = datetime.fromtimestamp(timestamp.seconds)
        
        # Log current state
        self._log_connection_state()
    
    async def _create_response(self, request):
        """Create a StreamReverseConnectionsResponse."""
        from google.protobuf.duration_pb2 import Duration
        
        # Import the actual proto classes dynamically to avoid compilation issues
        try:
            from envoy.service.reverse_tunnel.v3.rcrs_pb2 import StreamReverseConnectionsResponse
            
            response = StreamReverseConnectionsResponse()
            
            # Echo back the node info
            response.node.CopyFrom(request.node)
            
            # Set report interval
            response.report_interval.CopyFrom(Duration(seconds=self.report_interval_seconds))
            
            # Set request nonce for ACK
            response.request_nonce = request.nonce
            
            # No error_detail means this is an ACK
            
            logger.info(f"📤 Sending ACK response (interval={self.report_interval_seconds}s)")
            return response
            
        except ImportError:
            # Fallback: create a mock response object that gRPC can serialize
            logger.warning("Proto classes not available, creating mock response")
            
            # Create a simple mock response - this won't work with real gRPC
            # but shows what the response would contain
            mock_response = {
                'node': {'id': getattr(request.node, 'id', ''), 'cluster': getattr(request.node, 'cluster', '')},
                'report_interval': {'seconds': self.report_interval_seconds, 'nanos': 0},
                'request_nonce': getattr(request, 'nonce', ''),
            }
            
            logger.info(f"📤 Mock ACK response: {json.dumps(mock_response, indent=2)}")
            
            # For testing without proto compilation, we'll raise an exception
            raise RuntimeError("Proto classes required for actual gRPC communication. This is a demonstration.")
    
    def _log_connection_state(self):
        """Log current connection state."""
        logger.info("🔗 Current Connection State:")
        if not self.connected_nodes:
            logger.info("   (No connections)")
        else:
            for node_id, info in self.connected_nodes.items():
                status_icon = "🟢" if info['status'] == 'connected' else "🔴"
                logger.info(f"   {status_icon} {node_id}: {info['status']} "
                          f"(reported by {info['reported_by']} at {info['timestamp'].strftime('%H:%M:%S')})")
        logger.info("=" * 60)


class MockRCRSServer:
    """
    Mock server that demonstrates the RCRS protocol without actual gRPC.
    
    This server shows what requests and responses would look like and can be used
    to understand the protocol flow before implementing the real gRPC service.
    """
    
    def __init__(self, port: int = 9090, report_interval: int = 30):
        self.port = port
        self.report_interval = report_interval
        self.management_server = RCRSManagementServer(report_interval)
        
    async def run_mock_server(self):
        """Run a mock server that demonstrates the protocol."""
        logger.info(f"🚀 Starting Mock RCRS Management Server on port {self.port}")
        logger.info(f"📋 Report interval: {self.report_interval} seconds")
        logger.info("")
        logger.info("This server demonstrates what RCRS requests/responses would look like.")
        logger.info("For actual testing with Envoy, you'll need the proto-compiled version.")
        logger.info("")
        
        # Simulate some example requests
        await self._simulate_protocol_flow()
        
    async def _simulate_protocol_flow(self):
        """Simulate the RCRS protocol flow."""
        logger.info("📋 Simulating RCRS Protocol Flow:")
        logger.info("")
        
        # Create mock request objects
        mock_requests = [
            {
                'type': 'initial_handshake',
                'node': {'id': 'cloud-node', 'cluster': 'cloud'},
                'connections_info': [],
                'removed_connections': [],
                'listener_draining': False,
                'nonce': f'nonce_{int(time.time())}_1'
            },
            {
                'type': 'connection_added',
                'node': {'id': 'cloud-node', 'cluster': 'cloud'},
                'connections_info': [
                    {
                        'connection_identifier': {'node_id': 'on-prem-node'},
                        'timestamp': {'seconds': int(time.time()), 'nanos': 0}
                    }
                ],
                'removed_connections': [],
                'listener_draining': False,
                'nonce': f'nonce_{int(time.time())}_2'
            },
            {
                'type': 'periodic_report',
                'node': {'id': 'cloud-node', 'cluster': 'cloud'},
                'connections_info': [],
                'removed_connections': [],
                'listener_draining': False,
                'nonce': f'nonce_{int(time.time())}_3'
            },
            {
                'type': 'connection_removed',
                'node': {'id': 'cloud-node', 'cluster': 'cloud'},
                'connections_info': [],
                'removed_connections': [
                    {
                        'connection_identifier': {'node_id': 'on-prem-node'},
                        'timestamp': {'seconds': int(time.time()), 'nanos': 0}
                    }
                ],
                'listener_draining': False,
                'nonce': f'nonce_{int(time.time())}_4'
            }
        ]
        
        # Process each mock request
        for i, mock_request in enumerate(mock_requests, 1):
            logger.info(f"Step {i}: {mock_request['type']}")
            
            # Create a simple object to mimic the proto request
            class MockRequest:
                def __init__(self, data):
                    for key, value in data.items():
                        if key == 'node':
                            node = type('Node', (), {})()
                            node.id = value.get('id', '')
                            node.cluster = value.get('cluster', '')
                            setattr(self, key, node)
                        elif key == 'connections_info':
                            conn_list = []
                            for conn in value:
                                conn_obj = type('ConnInfo', (), {})()
                                conn_id = type('ConnId', (), {})()
                                conn_id.node_id = conn['connection_identifier']['node_id']
                                conn_obj.connection_identifier = conn_id
                                
                                timestamp = type('Timestamp', (), {})()
                                timestamp.seconds = conn['timestamp']['seconds']
                                timestamp.nanos = conn['timestamp']['nanos']
                                conn_obj.timestamp = timestamp
                                
                                conn_list.append(conn_obj)
                            setattr(self, key, conn_list)
                        elif key == 'removed_connections':
                            conn_list = []
                            for conn in value:
                                conn_obj = type('ConnInfo', (), {})()
                                conn_id = type('ConnId', (), {})()
                                conn_id.node_id = conn['connection_identifier']['node_id']
                                conn_obj.connection_identifier = conn_id
                                
                                timestamp = type('Timestamp', (), {})()
                                timestamp.seconds = conn['timestamp']['seconds']
                                timestamp.nanos = conn['timestamp']['nanos']
                                conn_obj.timestamp = timestamp
                                
                                conn_list.append(conn_obj)
                            setattr(self, key, conn_list)
                        else:
                            setattr(self, key, value)
            
            request = MockRequest(mock_request)
            
            try:
                await self.management_server._process_request(request)
                response = await self.management_server._create_response(request)
            except Exception as e:
                logger.info(f"📤 Mock response would be sent (proto compilation needed for actual response)")
                logger.info(f"    Response would contain: ACK for nonce {mock_request['nonce']}")
                logger.info(f"    Report interval: {self.report_interval} seconds")
            
            logger.info("")
            await asyncio.sleep(1)  # Brief pause between steps
        
        logger.info("✅ Mock protocol demonstration complete!")
        logger.info("")
        logger.info("🔧 To test with real Envoy:")
        logger.info("   1. Compile proto files: bazel build //api/envoy/service/reverse_tunnel/v3:rcrs_py_proto")
        logger.info("   2. Start this server: python rcrs_management_server.py --port 9090")
        logger.info("   3. Configure Envoy with RCRS cluster pointing to localhost:9090")
        logger.info("   4. Start Envoy and observe connection reports")


async def run_real_grpc_server(port: int, report_interval: int):
    """Run the actual gRPC server (requires proto compilation)."""
    try:
        from envoy.service.reverse_tunnel.v3.rcrs_pb2_grpc import (
            ReverseConnectionsReportingServiceServicer,
            add_ReverseConnectionsReportingServiceServicer_to_server
        )

        class RCRSServicer(ReverseConnectionsReportingServiceServicer):
            def __init__(self):
                self.management_server = RCRSManagementServer(report_interval)
            
            async def StreamReverseConnections(self, request_iterator, context):
                async for response in self.management_server.stream_reverse_connections(request_iterator, context):
                    yield response
        
        server = aio.server(futures.ThreadPoolExecutor(max_workers=10))
        servicer = RCRSServicer()
        add_ReverseConnectionsReportingServiceServicer_to_server(servicer, server)
        
        listen_addr = f'[::]:{port}'
        server.add_insecure_port(listen_addr)
        
        logger.info(f"🚀 Starting Real RCRS gRPC Server on {listen_addr}")
        logger.info(f"📋 Report interval: {report_interval} seconds")
        
        await server.start()
        await server.wait_for_termination()
        
    except ImportError as e:
        logger.error(f"❌ Cannot start real gRPC server: {e}")
        logger.error("Proto classes not available. Run mock server instead.")
        return False
    
    return True


def main():
    """Main function."""
    parser = argparse.ArgumentParser(description='RCRS Management Server')
    parser.add_argument('--port', type=int, default=9090,
                        help='Port to listen on (default: 9090)')
    parser.add_argument('--report-interval', type=int, default=30,
                        help='Report interval in seconds to send to clients (default: 30)')
    parser.add_argument('--mock', action='store_true',
                        help='Run mock server instead of real gRPC server')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose logging')
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    try:
        if args.mock:
            # Run mock server
            mock_server = MockRCRSServer(args.port, args.report_interval)
            asyncio.run(mock_server.run_mock_server())
        else:
            # Try to run real gRPC server
            success = asyncio.run(run_real_grpc_server(args.port, args.report_interval))
            if not success:
                logger.error("Failed to launch gRPC Server")
                
    except KeyboardInterrupt:
        logger.info("Server stopped by user")


if __name__ == '__main__':
    main()