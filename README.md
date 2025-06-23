# SteamDeck Motion Service
**JSON UDP Motion Server** for real-time gyroscope and accelerometer data from **Steam Deck**.

## Overview

This service continuously reads motion data from the Steam Deck's built-in IMU (Inertial Measurement Unit) and broadcasts it as JSON over UDP. Perfect for building motion-aware applications, accessibility tools, or motion sickness mitigation systems.

## Features

- **Real-time motion data** at 60Hz output (250Hz internal processing)
- **Simple JSON format** - easy to integrate with any programming language
- **UDP broadcasting** - multiple applications can receive data simultaneously
- **Robust frame handling** - automatic recovery from missed frames
- **Low latency** - optimized for real-time applications
- **Auto-start service** - runs automatically on boot

## Data Format

The service broadcasts JSON packets over UDP port **27760**:

```json
{
  "timestamp": 1672531200123,
  "accel": {"x": 0.15, "y": -0.03, "z": 0.98},
  "gyro": {"pitch": 2.1, "yaw": -0.5, "roll": 1.3},
  "frameId": 12456,
  "magnitude": {"accel": 1.02, "gyro": 2.7}
}
```

### Data Fields

- **timestamp**: Microseconds since epoch
- **accel**: Acceleration in G units (x=left/right, y=forward/back, z=up/down)
- **gyro**: Angular velocity in degrees/second (pitch, yaw, roll)
- **frameId**: Sequential frame counter for tracking
- **magnitude**: Total magnitude of acceleration and gyroscope vectors

## Installation

### Method 1: Quick Install (Recommended)
```bash
# Download and run the installer
curl -L https://github.com/yourusername/steamdeck-motion/releases/latest/download/install.sh | bash
```

### Method 2: Manual Installation
1. Download the latest release package
2. Extract to your desired location
3. Run `./install.sh`

The installer will:
- Copy the service binary to `~/sdmotion/`
- Install systemd service for auto-start
- Create desktop shortcuts for management

## Usage

### Starting the Service
```bash
systemctl --user start sdmotion
```

### Checking Status
```bash
systemctl --user status sdmotion
```

### Viewing Logs
```bash
journalctl --user -f -u sdmotion
```

### Testing Data Reception
```bash
# Listen for JSON data on port 27760
nc -u -l 27760
```

## Configuration

The service can be configured via environment variables:

```bash
# Custom UDP port (default: 27760)
export SDMOTION_SERVER_PORT=28000
systemctl --user restart sdmotion
```

## Development

### Building from Source

**Prerequisites:**
```bash
sudo pacman -S base-devel gcc glibc linux-api-headers ncurses systemd-libs hidapi
```

**Build:**
```bash
git clone https://github.com/yourusername/steamdeck-motion.git
cd steamdeck-motion
make release
```

**Install:**
```bash
make install
```

### Project Structure
```
inc/
├── motion/          # Simple motion data structures and JSON server
├── sdgyrodsu/       # Steam Deck HID frame processing
├── hiddev/          # HID device reading infrastructure
├── pipeline/        # Multi-threaded processing pipeline
└── log/             # Logging utilities

src/
├── motion/          # JSON server implementation
├── sdgyrodsu/       # Motion data processing
├── hiddev/          # HID device readers
├── pipeline/        # Threading and pipeline utilities
└── main.cpp         # Service entry point
```

## Use Cases

- **Motion sickness mitigation** - Detect rapid movements and apply visual cues
- **Accessibility tools** - Motion-based controls for users with limited mobility
- **Gaming enhancements** - Motion controls for emulators and games
- **Fitness tracking** - Monitor movement patterns and activity
- **VR/AR applications** - Use Steam Deck as motion controller
- **Research and prototyping** - Access to high-quality IMU data

## Client Examples

### Python Client
```python
import socket
import json

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('localhost', 27760))

while True:
    data, addr = sock.recvfrom(1024)
    motion = json.loads(data.decode())
    print(f"Accel: {motion['accel']}, Gyro: {motion['gyro']}")
```

### JavaScript/Node.js Client
```javascript
const dgram = require('dgram');
const client = dgram.createSocket('udp4');

client.bind(27760);
client.on('message', (msg) => {
    const motion = JSON.parse(msg.toString());
    console.log(`Accel: ${JSON.stringify(motion.accel)}`);
});
```

## Troubleshooting

### Service Won't Start
```bash
# Check service status
systemctl --user status sdmotion

# Check permissions (should have hidraw access)
ls -la /dev/hidraw*

# Check logs for errors
journalctl --user -u sdmotion
```

### No Data Received
```bash
# Verify service is broadcasting
ss -ulpn | grep 27760

# Test with netcat
nc -u -l 27760

# Check firewall (if using remote access)
sudo ufw allow 27760/udp
```

### High CPU Usage
The service is optimized for low CPU usage. High usage may indicate:
- Hardware issues with the IMU
- Multiple applications reading data simultaneously
- Log level set too high

## Technical Details

- **HID Interface**: Uses USB interface 2 of Steam Deck controller (VID: 0x28de, PID: 0x1205)
- **Data Rate**: 250Hz internal processing, 60Hz UDP output
- **Accuracy**: 16-bit accelerometer and gyroscope data
- **Latency**: < 5ms from hardware to UDP packet
- **Threading**: Multi-threaded pipeline for optimal performance

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions welcome! Please read the contributing guidelines and submit pull requests.

## Acknowledgments

Based on the excellent SteamDeckGyroDSU project by kmicki, adapted for general-purpose motion data access.