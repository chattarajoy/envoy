#!/usr/bin/env python3
"""
Test client to send messages to the RCRS server and see if it works.
"""

import asyncio
import logging
import sys
import time

import grpc
from grpc import aio

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


async def test_client(server_address: str = "localhost:9090"):
    """Test client that sends messages to the RCRS server."""
    try:
        from envoy.service.reverse_tunnel.v3 import rcrs_pb2, rcrs_pb2_grpc
        from envoy.config.core.v3 import base_pb2
        from google.protobuf import timestamp_pb2
        
        logger.info(f"🚀 Connecting to RCRS server at {server_address}")
        
        # Create a channel
        async with aio.insecure_channel(server_address) as channel:
            # Create the stub
            stub = rcrs_pb2_grpc.ReverseConnectionsReportingServiceStub(channel)
            
            # Create a test request
            request = rcrs_pb2.StreamReverseConnectionsRequest()
            
            # Set the node info
            request.node.id = "test-client-node"
            request.node.cluster = "test-cluster"
            
            # Set a nonce
            request.nonce = f"test-nonce-{int(time.time())}"
            
            # Set listener draining to false
            request.listener_draining = False
            
            # Create a test connection info
            conn_info = request.connections_info.add()
            conn_info.connection_identifier.tenant_id = "default"
            conn_info.connection_identifier.cluster_id = "on-prem-cluster"
            conn_info.connection_identifier.node_id = "on-prem-node"
            
            # Set timestamp
            conn_info.timestamp.seconds = int(time.time())
            conn_info.timestamp.nanos = 0
            
            logger.info(f"📤 Sending test request:")
            logger.info(f"   Node ID: {request.node.id}")
            logger.info(f"   Cluster: {request.node.cluster}")
            logger.info(f"   Nonce: {request.nonce}")
            logger.info(f"   Connections: {len(request.connections_info)}")
            
            # Create an async generator for the request
            async def request_generator():
                yield request
                # Wait a bit and send another request
                await asyncio.sleep(1)
                request2 = rcrs_pb2.StreamReverseConnectionsRequest()
                request2.node.id = "test-client-node"
                request2.node.cluster = "test-cluster"
                request2.nonce = f"test-nonce-{int(time.time())}"
                request2.listener_draining = False
                yield request2
            
            # Call the streaming RPC
            logger.info("🔄 Starting streaming RPC...")
            async for response in stub.StreamReverseConnections(request_generator()):
                logger.info(f"📥 Received response:")
                logger.info(f"   Request nonce: {response.request_nonce}")
                logger.info(f"   Report interval: {response.report_interval.seconds}s")
                
    except ImportError as e:
        logger.error(f"❌ Import error: {e}")
        logger.error("Proto classes not available.")
        return False
    except Exception as e:
        logger.error(f"❌ Error in test client: {e}")
        import traceback
        logger.error(f"Full traceback: {traceback.format_exc()}")
        return False
    
    return True


def main():
    """Main function."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Test RCRS Client')
    parser.add_argument('--server', type=str, default="localhost:9090",
                        help='Server address (default: localhost:9090)')
    
    args = parser.parse_args()
    
    try:
        success = asyncio.run(test_client(args.server))
        if success:
            logger.info("✅ Test client completed successfully")
        else:
            logger.error("❌ Test client failed")
            
    except KeyboardInterrupt:
        logger.info("Test client stopped by user")


if __name__ == '__main__':
    main()
