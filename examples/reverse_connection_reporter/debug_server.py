#!/usr/bin/env python3
"""
Debug server to capture raw gRPC messages and see what's being sent.
"""

import asyncio
import logging
import sys
import traceback
from concurrent import futures

import grpc
from grpc import aio

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class DebugServicer:
    """Debug servicer that tries to capture raw message data."""
    
    async def StreamReverseConnections(self, request_iterator, context):
        """Handle the bidirectional StreamReverseConnections RPC with debug info."""
        peer_info = context.peer()
        logger.info(f"🚀 New RCRS stream from {peer_info}")
        
        try:
            # Try to get the raw request iterator
            logger.info(f"Request iterator type: {type(request_iterator)}")
            logger.info(f"Request iterator attributes: {dir(request_iterator)}")
            
            # Try to iterate and catch the error
            request_count = 0
            async for request in request_iterator:
                request_count += 1
                logger.info(f"✅ Successfully received request #{request_count}")
                logger.info(f"Request type: {type(request)}")
                logger.info(f"Request attributes: {dir(request)}")
                
                # Try to print some basic info about the request
                try:
                    if hasattr(request, 'node'):
                        logger.info(f"Request has node: {request.node}")
                    if hasattr(request, 'nonce'):
                        logger.info(f"Request nonce: {request.nonce}")
                    if hasattr(request, 'connections_info'):
                        logger.info(f"Request connections_info: {len(request.connections_info)} items")
                    if hasattr(request, 'removed_connections'):
                        logger.info(f"Request removed_connections: {len(request.removed_connections)} items")
                except Exception as e:
                    logger.error(f"Error accessing request fields: {e}")
                
                # Create a simple response
                try:
                    from envoy.service.reverse_tunnel.v3 import rcrs_pb2
                    response = rcrs_pb2.StreamReverseConnectionsResponse()
                    response.request_nonce = getattr(request, 'nonce', 'debug-nonce')
                    logger.info(f"📤 Sending response for nonce: {response.request_nonce}")
                    yield response
                except Exception as e:
                    logger.error(f"Error creating response: {e}")
                    # Create a mock response
                    class MockResponse:
                        def __init__(self):
                            self.request_nonce = 'mock-nonce'
                    yield MockResponse()
                    
        except Exception as e:
            logger.error(f"❌ Error in debug stream: {e}")
            logger.error(f"Exception type: {type(e).__name__}")
            logger.error(f"Exception args: {e.args}")
            logger.error(f"Full traceback: {traceback.format_exc()}")
            
            # Try to get more details about the raw data
            try:
                # Check if we can access the raw bytes somehow
                if hasattr(e, '__dict__'):
                    logger.error(f"Exception dict: {e.__dict__}")
                if hasattr(e, 'message'):
                    logger.error(f"Exception message: {e.message}")
                if hasattr(e, 'details'):
                    logger.error(f"Exception details: {e.details}")
            except Exception as debug_e:
                logger.error(f"Could not extract debug info: {debug_e}")
        finally:
            logger.info(f"✅ Debug stream ended for {peer_info}")


async def run_debug_server(port: int = 9090):
    """Run the debug gRPC server."""
    try:
        from envoy.service.reverse_tunnel.v3 import rcrs_pb2_grpc
        
        server = aio.server(futures.ThreadPoolExecutor(max_workers=10))
        servicer = DebugServicer()
        rcrs_pb2_grpc.add_ReverseConnectionsReportingServiceServicer_to_server(servicer, server)
        
        listen_addr = f'[::]:{port}'
        server.add_insecure_port(listen_addr)
        
        logger.info(f"🚀 Starting Debug RCRS gRPC Server on {listen_addr}")
        
        await server.start()
        await server.wait_for_termination()
        
    except ImportError as e:
        logger.error(f"❌ Cannot start debug gRPC server: {e}")
        logger.error("Proto classes not available.")
        return False
    
    return True


def main():
    """Main function."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Debug RCRS Management Server')
    parser.add_argument('--port', type=int, default=9090,
                        help='Port to listen on (default: 9090)')
    
    args = parser.parse_args()
    
    try:
        success = asyncio.run(run_debug_server(args.port))
        if not success:
            logger.error("Failed to launch debug gRPC Server")
            
    except KeyboardInterrupt:
        logger.info("Debug server stopped by user")


if __name__ == '__main__':
    main()
