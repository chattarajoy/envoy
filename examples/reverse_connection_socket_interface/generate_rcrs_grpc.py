#!/usr/bin/env python3
"""
Script to generate Python gRPC bindings for rcrs.proto and install them in xds-protos

This script:
1. Installs xds-protos from PyPI
2. Generates the Python gRPC service stubs for the Reverse Connection 
   Reporting Service (RCRS) proto file
3. Places them in the xds-protos site-packages directory structure

Usage:
    python3 examples/reverse_connection_socket_interface/generate_rcrs_grpc.py
"""

import os
import subprocess
import sys
import site
from pathlib import Path

def install_xds_protos():
    """Install xds-protos from PyPI if not already installed."""
    try:
        import xds
        print("✅ xds-protos already installed")
        return True
    except ImportError:
        print("📦 Installing xds-protos from PyPI...")
        try:
            result = subprocess.run([
                sys.executable, "-m", "pip", "install", "xds-protos"
            ], capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ Successfully installed xds-protos")
                return True
            else:
                print("❌ Failed to install xds-protos:")
                print(result.stderr)
                return False
        except Exception as e:
            print(f"❌ Error installing xds-protos: {e}")
            return False

def find_xds_protos_location():
    """Find the xds-protos installation location in site-packages."""
    # Refresh site packages after potential installation
    site.main()
    
    for site_dir in site.getsitepackages() + [site.getusersitepackages()]:
        if site_dir and Path(site_dir).exists():
            xds_path = Path(site_dir) / "xds"
            if xds_path.exists():
                return xds_path
    
    # Fallback: try to import and find the path
    try:
        import xds
        xds_file = Path(xds.__file__)
        return xds_file.parent
    except ImportError:
        return None

def main():
    # Get the workspace root (where this script is run from)
    workspace_root = Path.cwd()
    
    # Define paths
    proto_file = "envoy/service/reverse_tunnel/v3/rcrs.proto"
    
    # Proto include paths - using the external dependencies from bazel
    proto_paths = [
        "api",
        "bazel-envoy/external/com_google_protobuf/src",
        "bazel-envoy/external/com_google_googleapis", 
        "bazel-envoy/external/com_envoyproxy_protoc_gen_validate",
        "bazel-envoy/external/com_github_cncf_xds",
        "bazel-envoy/external/envoy_api"
    ]
    
    # Step 1: Install xds-protos
    if not install_xds_protos():
        print("❌ Cannot proceed without xds-protos")
        sys.exit(1)
    
    # Step 2: Find xds-protos installation location
    xds_location = find_xds_protos_location()
    if not xds_location:
        print("❌ Could not find xds-protos installation location")
        sys.exit(1)
    
    print(f"📍 Found xds-protos at: {xds_location}")
    
    # Step 3: Create output directory in xds-protos
    output_dir = xds_location.parent  # This should be site-packages
    envoy_output_dir = output_dir / "envoy"
    
    # Create the envoy directory structure in site-packages
    envoy_service_dir = envoy_output_dir / "service" / "reverse_tunnel" / "v3"
    envoy_service_dir.mkdir(parents=True, exist_ok=True)
    
    # Create __init__.py files for proper Python package structure
    init_files = [
        envoy_output_dir / "__init__.py",
        envoy_output_dir / "service" / "__init__.py", 
        envoy_output_dir / "service" / "reverse_tunnel" / "__init__.py",
        envoy_output_dir / "service" / "reverse_tunnel" / "v3" / "__init__.py"
    ]
    
    for init_file in init_files:
        if not init_file.exists():
            init_file.write_text("# Envoy API Python bindings\n")
    
    print(f"📁 Created output directory: {envoy_service_dir}")
    
    # Make sure the directory is writable
    try:
        # Test write permissions by actually trying to create a proto file
        test_file = envoy_service_dir / "test_rcrs_pb2.py"
        test_file.write_text("# Test file")
        test_file.unlink()
        
        # Check if we can modify existing files
        if (envoy_service_dir / "rcrs_pb2.py").exists():
            os.chmod(envoy_service_dir / "rcrs_pb2.py", 0o644)
        if (envoy_service_dir / "rcrs_pb2_grpc.py").exists():
            os.chmod(envoy_service_dir / "rcrs_pb2_grpc.py", 0o644)
            
    except (PermissionError, OSError) as e:
        print(f"⚠️  Permission issue with site-packages directory: {e}")
        # Fallback to user site-packages
        user_site = Path(site.getusersitepackages())
        user_site.mkdir(parents=True, exist_ok=True)
        output_dir = user_site
        envoy_output_dir = output_dir / "envoy"
        envoy_service_dir = envoy_output_dir / "service" / "reverse_tunnel" / "v3"
        envoy_service_dir.mkdir(parents=True, exist_ok=True)
        
        # Recreate __init__.py files in user site-packages
        init_files = [
            envoy_output_dir / "__init__.py",
            envoy_output_dir / "service" / "__init__.py", 
            envoy_output_dir / "service" / "reverse_tunnel" / "__init__.py",
            envoy_output_dir / "service" / "reverse_tunnel" / "v3" / "__init__.py"
        ]
        
        for init_file in init_files:
            if not init_file.exists():
                init_file.write_text("# Envoy API Python bindings\n")
        
        print(f"📁 Using user site-packages: {envoy_service_dir}")
    
    # Step 4: Check prerequisites
    proto_file_path = workspace_root / "api" / proto_file
    if not proto_file_path.exists():
        print(f"❌ Proto file not found: {proto_file_path}")
        print("Please ensure you're running this from the Envoy workspace root")
        sys.exit(1)
    
    # Verify proto paths exist
    missing_paths = []
    for path in proto_paths:
        full_path = workspace_root / path
        if not full_path.exists():
            missing_paths.append(path)
    
    if missing_paths:
        print("⚠️  Some proto paths don't exist:")
        for path in missing_paths:
            print(f"  - {path}")
        print("This might cause import errors during generation.")
        print("Consider running: bazel build @envoy_api//envoy/service/reverse_tunnel/v3:pkg_py_proto")
    
    # Step 5: Install grpcio-tools if needed
    try:
        import grpc_tools.protoc
    except ImportError:
        print("📦 Installing grpcio-tools...")
        result = subprocess.run([
            sys.executable, "-m", "pip", "install", "grpcio-tools"
        ], capture_output=True, text=True)
        
        if result.returncode != 0:
            print("❌ Failed to install grpcio-tools")
            print(result.stderr)
            sys.exit(1)
        print("✅ Installed grpcio-tools")
    
    # Step 6: Build the protoc command
    cmd = [
        sys.executable, "-m", "grpc_tools.protoc"
    ]
    
    # Add proto paths
    for path in proto_paths:
        cmd.extend(["--proto_path", str(path)])
    
    # Add output options - generate in xds-protos site-packages
    cmd.extend([
        "--python_out", str(output_dir),
        "--grpc_python_out", str(output_dir)
    ])
    
    # Add the proto file
    cmd.append(proto_file)
    
    print("🔧 Generating Python gRPC bindings for rcrs.proto...")
    print(f"Command: {' '.join(cmd)}")
    print()
    
    try:
        # Run the protoc command
        result = subprocess.run(cmd, cwd=workspace_root, capture_output=True, text=True)
        
        if result.returncode == 0:
            print("✅ Successfully generated Python gRPC bindings!")
            
            # Check what files were generated
            grpc_file = envoy_service_dir / "rcrs_pb2_grpc.py"
            proto_file_out = envoy_service_dir / "rcrs_pb2.py"
            
            if grpc_file.exists():
                print(f"✅ Generated: {grpc_file}")
            else:
                print(f"⚠️  Expected file not found: {grpc_file}")
                
            if proto_file_out.exists():
                print(f"✅ Generated: {proto_file_out}")
            else:
                print(f"⚠️  Expected file not found: {proto_file_out}")
            
            print(f"\n🎉 Files installed in xds-protos site-packages!")
            print(f"📍 Location: {envoy_service_dir}")
            print("\n💡 Usage:")
            print("   from envoy.service.reverse_tunnel.v3 import rcrs_pb2")
            print("   from envoy.service.reverse_tunnel.v3 import rcrs_pb2_grpc")
                
            if result.stderr:
                print("\n⚠️  Warnings/Info:")
                print(result.stderr)
                
        else:
            print("❌ Failed to generate Python gRPC bindings!")
            print("Error output:")
            print(result.stderr)
            if result.stdout:
                print("Standard output:")
                print(result.stdout)
            sys.exit(1)
            
    except FileNotFoundError:
        print("❌ Error: grpcio-tools not found!")
        print("This should have been installed automatically. Please try manually:")
        print("pip install grpcio-tools")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()