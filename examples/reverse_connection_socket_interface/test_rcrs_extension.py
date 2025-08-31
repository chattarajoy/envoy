#!/usr/bin/env python3
"""
Test script to verify the ReverseConnectionsReporter bootstrap extension is working.
"""

import requests
import json
import time
import sys

def test_rcrs_extension():
    """Test that the RCRS extension is loaded and configured."""
    
    # Wait for Envoy to start up
    print("Waiting for Envoy to start...")
    time.sleep(5)
    
    try:
        # Check if Envoy admin interface is accessible
        admin_url = "http://localhost:8889"
        response = requests.get(f"{admin_url}/ready", timeout=10)
        if response.status_code != 200:
            print(f"❌ Envoy not ready. Status: {response.status_code}")
            return False
        
        print("✅ Envoy is ready")
        
        # Check bootstrap extensions
        response = requests.get(f"{admin_url}/config_dump", timeout=10)
        if response.status_code != 200:
            print(f"❌ Failed to get config dump. Status: {response.status_code}")
            return False
        
        config = response.json()
        
        # Look for bootstrap extensions in the config
        bootstrap_extensions = []
        if 'configs' in config:
            for config_item in config['configs']:
                if config_item.get('@type') == 'type.googleapis.com/envoy.admin.v3.BootstrapConfigDump':
                    bootstrap = config_item.get('bootstrap', {})
                    bootstrap_extensions = bootstrap.get('bootstrap_extensions', [])
                    break
        
        print(f"Found {len(bootstrap_extensions)} bootstrap extensions:")
        
        rcrs_extension_found = False
        for ext in bootstrap_extensions:
            name = ext.get('name', '')
            print(f"  - {name}")
            if name == 'envoy.bootstrap.reverse_connection.reverse_connections_reporter':
                rcrs_extension_found = True
                typed_config = ext.get('typed_config', {})
                print(f"    Config: {json.dumps(typed_config, indent=4)}")
        
        if rcrs_extension_found:
            print("✅ RCRS extension found and configured")
        else:
            print("❌ RCRS extension not found")
            return False
        
        # Check if the rcrs_cluster is configured
        clusters_found = False
        for config_item in config['configs']:
            if config_item.get('@type') == 'type.googleapis.com/envoy.admin.v3.ClustersConfigDump':
                clusters = config_item.get('dynamic_active_clusters', [])
                for cluster in clusters:
                    cluster_name = cluster.get('cluster', {}).get('name', '')
                    if cluster_name == 'rcrs_cluster':
                        clusters_found = True
                        print(f"✅ RCRS cluster found: {cluster_name}")
                        break
        
        if not clusters_found:
            print("❌ RCRS cluster not found")
            return False
        
        print("✅ All RCRS components are properly configured!")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"❌ Failed to connect to Envoy admin interface: {e}")
        return False
    except Exception as e:
        print(f"❌ Test failed with error: {e}")
        return False

if __name__ == "__main__":
    success = test_rcrs_extension()
    sys.exit(0 if success else 1)
