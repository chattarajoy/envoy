#!/bin/bash
# Test script for RCRS (Reverse Connection Reporting Service) feature
# This script starts the management server and provides instructions for running Envoy

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENVOY_CONFIG="$SCRIPT_DIR/envoy-with-rcrs.yaml"
MANAGEMENT_SERVER="$SCRIPT_DIR/rcrs_management_server.py"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 RCRS Feature Test Setup${NC}"
echo "=================================="
echo

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a port is in use
port_in_use() {
    lsof -Pi :$1 -sTCP:LISTEN -t >/dev/null 2>&1
}

# Function to cleanup background processes
cleanup() {
    echo -e "\n${YELLOW}🧹 Cleaning up...${NC}"
    if [[ ! -z "$MANAGEMENT_SERVER_PID" ]]; then
        kill $MANAGEMENT_SERVER_PID 2>/dev/null || true
        echo "Management server stopped"
    fi
    if [[ ! -z "$ENVOY_PID" ]]; then
        kill $ENVOY_PID 2>/dev/null || true
        echo "Envoy stopped"
    fi
}

# Set up signal handlers
trap cleanup EXIT INT TERM

# Check dependencies
echo -e "${BLUE}📋 Checking dependencies...${NC}"

if ! command_exists python3; then
    echo -e "${RED}❌ Python 3 is required${NC}"
    exit 1
fi

if ! python3 -c "import grpc, asyncio" 2>/dev/null; then
    echo -e "${YELLOW}⚠️  Installing Python dependencies...${NC}"
    pip3 install grpcio grpcio-tools asyncio
fi

# Check if ports are available
if port_in_use 9090; then
    echo -e "${RED}❌ Port 9090 is already in use (needed for RCRS server)${NC}"
    exit 1
fi

if port_in_use 9901; then
    echo -e "${YELLOW}⚠️  Port 9901 is in use (Envoy admin interface)${NC}"
fi

if port_in_use 9902; then
    echo -e "${YELLOW}⚠️  Port 9902 is in use (Envoy admin interface backup)${NC}"
fi

echo -e "${GREEN}✅ Dependencies check passed${NC}"
echo

# Start management server
echo -e "${BLUE}🖥️  Starting RCRS Management Server...${NC}"
echo "   Server will listen on localhost:9090"
echo "   Report interval: 10 seconds"
echo

# Start management server in background (mock mode for demo)
python3 "$MANAGEMENT_SERVER" --port 9090 --report-interval 10 --mock &
MANAGEMENT_SERVER_PID=$!

# Wait a moment for server to start
sleep 2

# Check if management server started successfully
if ! kill -0 $MANAGEMENT_SERVER_PID 2>/dev/null; then
    echo -e "${RED}❌ Failed to start management server${NC}"
    exit 1
fi

echo -e "${GREEN}✅ RCRS Management Server started (PID: $MANAGEMENT_SERVER_PID)${NC}"
echo

# Show Envoy configuration info
echo -e "${BLUE}📄 Envoy Configuration Ready${NC}"
echo "   Config file: $ENVOY_CONFIG"
echo "   Admin interface: http://localhost:9902/stats (when Envoy is running)"
echo "   RCRS cluster points to: localhost:9090"
echo

# Instructions for running Envoy
echo -e "${YELLOW}🔧 To test RCRS with your built Envoy:${NC}"
echo
echo "1. Build Envoy (in another terminal):"
echo "   cd $SCRIPT_DIR/../.."
echo "   bazel build //source/exe:envoy-static"
echo
echo "2. Run Envoy with RCRS config:"
echo "   ./bazel-bin/source/exe/envoy-static -c $ENVOY_CONFIG --log-level debug"
echo
echo "3. If RCRS support is not yet integrated, edit the config file:"
echo "   # Uncomment the rcrs_config section in $ENVOY_CONFIG"
echo "   # Look for the 'rcrs_config:' section under bootstrap_extensions"
echo
echo "4. Monitor the management server output above for RCRS messages"
echo
echo "5. Check Envoy metrics for RCRS stats:"
echo "   curl http://localhost:9902/stats | grep reverse_connection_reporter"
echo

# Show what to expect
echo -e "${BLUE}📊 Expected RCRS Metrics:${NC}"
cat << EOF
   reverse_connection_reporter.requests
   reverse_connection_reporter.responses  
   reverse_connection_reporter.errors
   reverse_connection_reporter.retries
   reverse_connection_reporter.connections_added
   reverse_connection_reporter.connections_removed
EOF
echo

# Show expected log messages
echo -e "${BLUE}🔍 Expected Behaviors:${NC}"
echo "   • Management server should log incoming RCRS connections"
echo "   • Envoy logs should show gRPC stream establishment to localhost:9090"
echo "   • Connection state changes should be reported to management server"
echo "   • Management server should ACK each report with configured interval"
echo

# Interactive mode
echo -e "${BLUE}🎛️  Interactive Mode${NC}"
echo "Management server is running. Press 'q' to quit or 'h' for help."
echo

# Simple interactive loop
while true; do
    read -n 1 -s key
    case $key in
        'q'|'Q')
            echo -e "\n${YELLOW}Stopping test setup...${NC}"
            break
            ;;
        'h'|'H')
            echo -e "\n${BLUE}📋 Available commands:${NC}"
            echo "   q - Quit and cleanup"
            echo "   h - Show this help"
            echo "   s - Show server status"
            echo "   c - Show Envoy config location"
            echo "   m - Show expected metrics"
            echo
            ;;
        's'|'S')
            echo -e "\n${BLUE}📊 Server Status:${NC}"
            if kill -0 $MANAGEMENT_SERVER_PID 2>/dev/null; then
                echo -e "   Management Server: ${GREEN}Running${NC} (PID: $MANAGEMENT_SERVER_PID)"
            else
                echo -e "   Management Server: ${RED}Stopped${NC}"
            fi
            echo "   RCRS Port: 9090"
            echo "   Envoy Admin Port: 9902 (when Envoy is running)"
            echo
            ;;
        'c'|'C')
            echo -e "\n${BLUE}📄 Configuration:${NC}"
            echo "   Envoy config: $ENVOY_CONFIG"
            echo "   Management server: $MANAGEMENT_SERVER"
            echo "   To edit RCRS settings: uncomment rcrs_config section in Envoy config"
            echo
            ;;
        'm'|'M')
            echo -e "\n${BLUE}📊 RCRS Metrics to Monitor:${NC}"
            echo "   curl http://localhost:9902/stats | grep reverse_connection_reporter"
            echo "   - requests: Total RCRS requests sent"
            echo "   - responses: Total RCRS responses received"
            echo "   - errors: RCRS communication errors"
            echo "   - retries: RCRS retry attempts"
            echo "   - connections_added: Connections reported as added"
            echo "   - connections_removed: Connections reported as removed"
            echo
            ;;
        *)
            # Ignore other keys
            ;;
    esac
done

echo -e "${GREEN}✅ RCRS test setup completed${NC}"