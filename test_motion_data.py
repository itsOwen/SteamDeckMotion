#!/usr/bin/env python3
"""
Test script to verify Steam Deck Motion Service is working.
Run this to see live motion data from your Steam Deck.
"""

import socket
import json
import sys
import time
from datetime import datetime

def test_motion_service(port=27760, duration=10):
    """Test the motion service by listening for UDP packets."""
    
    print(f"Steam Deck Motion Service Test")
    print(f"Listening on UDP port {port} for {duration} seconds...")
    print("Move your Steam Deck to see motion data!")
    print("-" * 60)
    
    try:
        # Create UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2.0)  # 2 second timeout
        
        # Send a registration packet (any packet to register as client)
        sock.sendto(b"register", ('localhost', port))
        
        # Bind to receive data
        sock.bind(('', port + 1))  # Use different port for receiving
        
        start_time = time.time()
        packet_count = 0
        last_frame_id = 0
        
        while time.time() - start_time < duration:
            try:
                # Receive data
                data, addr = sock.recvfrom(1024)
                packet_count += 1
                
                # Parse JSON
                try:
                    motion = json.loads(data.decode('utf-8'))
                    
                    # Extract data
                    timestamp = motion.get('timestamp', 0)
                    accel = motion.get('accel', {})
                    gyro = motion.get('gyro', {})
                    frame_id = motion.get('frameId', 0)
                    magnitude = motion.get('magnitude', {})
                    
                    # Check for frame drops
                    if last_frame_id > 0 and frame_id > last_frame_id + 1:
                        dropped = frame_id - last_frame_id - 1
                        print(f"⚠️  Dropped {dropped} frames")
                    
                    last_frame_id = frame_id
                    
                    # Format timestamp
                    dt = datetime.fromtimestamp(timestamp / 1_000_000)
                    time_str = dt.strftime("%H:%M:%S.%f")[:-3]
                    
                    # Display data
                    print(f"[{time_str}] Frame {frame_id:6d} | "
                          f"Accel: X={accel.get('x', 0):6.3f} Y={accel.get('y', 0):6.3f} Z={accel.get('z', 0):6.3f} "
                          f"(|{magnitude.get('accel', 0):5.3f}|) | "
                          f"Gyro: P={gyro.get('pitch', 0):6.1f} Y={gyro.get('yaw', 0):6.1f} R={gyro.get('roll', 0):6.1f} "
                          f"(|{magnitude.get('gyro', 0):5.1f}|)")
                    
                except json.JSONDecodeError as e:
                    print(f"❌ JSON decode error: {e}")
                    print(f"   Raw data: {data}")
                    
            except socket.timeout:
                print("⏱️  No data received (timeout)")
                continue
            except KeyboardInterrupt:
                break
                
    except Exception as e:
        print(f"❌ Error: {e}")
        return False
    finally:
        sock.close()
    
    print("-" * 60)
    print(f"✅ Test completed! Received {packet_count} packets in {duration} seconds")
    print(f"   Average rate: {packet_count/duration:.1f} packets/second")
    
    if packet_count == 0:
        print("\n❌ No data received. Check that:")
        print("   1. Motion service is running: systemctl --user status sdmotion")
        print("   2. Service is bound to correct port")
        print("   3. No firewall blocking UDP traffic")
        return False
    
    return True

def check_service_status():
    """Check if the motion service is running."""
    import subprocess
    
    try:
        result = subprocess.run(['systemctl', '--user', 'is-active', 'sdmotion'], 
                              capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip() == 'active':
            print("✅ Motion service is running")
            return True
        else:
            print("❌ Motion service is not running")
            print("   Start it with: systemctl --user start sdmotion")
            return False
    except Exception as e:
        print(f"⚠️  Could not check service status: {e}")
        return True  # Continue anyway

if __name__ == "__main__":
    print("Steam Deck Motion Service Test Tool")
    print("==================================")
    
    # Check service status first
    if not check_service_status():
        sys.exit(1)
    
    # Parse command line arguments
    port = 27760
    duration = 10
    
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print("Invalid port number")
            sys.exit(1)
    
    if len(sys.argv) > 2:
        try:
            duration = int(sys.argv[2])
        except ValueError:
            print("Invalid duration")
            sys.exit(1)
    
    # Run test
    success = test_motion_service(port, duration)
    sys.exit(0 if success else 1)