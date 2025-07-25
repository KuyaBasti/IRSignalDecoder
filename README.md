# IR Signal Decoder - Text Messaging System for CC3200

An advanced IR remote control-based text messaging system for the Texas Instruments CC3200 microcontroller, featuring real-time message composition and device-to-device communication via OLED display and UART.

![Project Demo](https://img.shields.io/badge/Platform-CC3200-blue) ![Language](https://img.shields.io/badge/Language-C-brightgreen) ![Display](https://img.shields.io/badge/Display-SSD1351%20OLED-orange) ![Interface](https://img.shields.io/badge/Input-IR%20Remote-red)

## 🎯 Overview

This project transforms a standard IR remote control into a sophisticated text input device for embedded messaging. The system provides:

- **IR Remote Text Input**: Use numeric keypad and navigation buttons for message composition
- **Real-time OLED Display**: 128x128 color display shows messages, menus, and status
- **Inter-device Communication**: Send and receive messages between multiple CC3200 units via UART
- **Complete Messaging Interface**: Compose, edit, send, and receive text messages
- **User-friendly Navigation**: Intuitive menu system with visual feedback

## 🏗️ Hardware Requirements

### Primary Components
- **Texas Instruments CC3200 Development Board**
- **1.5" 128x128 RGB OLED Display (SSD1351 driver)**
- **IR Receiver Module** (38kHz, compatible with standard TV remotes)
- **Standard IR Remote Control** (with numeric keypad 0-9, LAST, MUTE buttons)

### Pin Configuration
| Function | CC3200 Pin | Connection |
|----------|------------|------------|
| SPI MOSI | Pin 7      | OLED Data  |
| SPI CLK  | Pin 5      | OLED Clock |
| SPI CS   | Pin 8      | OLED CS    |
| IR Data  | Pin 4      | IR Receiver Output |
| UART TX  | Pin 55     | Inter-device TX |
| UART RX  | Pin 57     | Inter-device RX |
| UART TX  | Pin 1      | Debug Output |
| UART RX  | Pin 2      | Debug Input |

## 🛠️ Software Architecture

### Core Components

#### 1. **Main Application** (`main.c`)
- **IR Signal Decoder**: NEC protocol decoder for remote control signals
- **Message Engine**: Text composition, editing, and transmission logic
- **State Machine**: Menu navigation and application flow control
- **Communication Handler**: UART-based inter-device messaging
- **User Interface**: OLED-based visual interface management

#### 2. **Graphics Library** (`Adafruit_GFX.c/h`)
- **Text Rendering**: Multi-line text display with word wrapping
- **Menu Graphics**: Button highlights, selection indicators, status bars
- **Message Display**: Scrollable message lists and conversation views
- **Color Management**: 16-bit RGB565 color space for visual feedback

#### 3. **OLED Driver** (`Adafruit_OLED.c`, `Adafruit_SSD1351.h`)
- **SSD1351 Controller**: Full driver implementation for 128x128 OLED
- **Fast Updates**: Optimized screen refresh for responsive UI
- **Text Modes**: Support for different font sizes and colors
- **Graphics Primitives**: Lines, rectangles, and filled shapes

#### 4. **UART Interface** (`uart_if.c`)
- **Dual UART Support**: Debug console + inter-device communication
- **Message Protocol**: Structured data transmission between devices
- **Buffered I/O**: Reliable data transfer with error handling
- **Flow Control**: Proper handshaking for message delivery

### Key Features

#### IR Remote Control Mapping
```c
// Button assignments for text input
Button 0-9:  Numeric input and T9-style text entry
LAST:        Backspace/Delete
MUTE:        Send message
VOL+:        Navigate up/select
VOL-:        Navigate down/back
CH+:         Switch input mode
CH-:         Access menu
```

#### Message Composition System
```c
// T9-style text input using numeric keypad
2: ABC    3: DEF    4: GHI    5: JKL    6: MNO
7: PQRS   8: TUV    9: WXYZ   0: Space  1: Punctuation
```

#### Inter-device Communication Protocol
```c
typedef struct {
    uint8_t message_type;    // TEXT, ACK, STATUS
    uint8_t sender_id;       // Device identifier
    uint16_t message_length; // Payload size
    char payload[MAX_MSG_LEN]; // Message content
    uint8_t checksum;        // Error detection
} MessagePacket;
```

## 🚀 Quick Start

### Prerequisites
- **Code Composer Studio (CCS)** v12.0 or later
- **CC3200 SDK** installed and configured
- **IR Remote Control** (any standard TV/STB remote)
- **Hardware setup** as described above

### Build Instructions

1. **Clone/Download** this repository to your local machine

2. **Import Project** into Code Composer Studio:
   ```bash
   File → Import → Code Composer Studio → CCS Projects
   Select the project directory
   ```

3. **Configure Target**:
   - Right-click project → Properties
   - Select CC3200 target configuration
   - Verify linker command file: `cc3200v1p32.cmd`

4. **Build Project**:
   ```bash
   Project → Build All
   ```
   Or use keyboard shortcut: `Ctrl+B`

5. **Flash and Run**:
   - Connect CC3200 via USB
   - Debug → Debug As → Code Composer Studio → CC3200
   - Press F8 to run

### Initial Setup

#### IR Remote Calibration
1. **Power on** the device - OLED will show startup screen
2. **Press any button** on your IR remote to auto-detect format
3. **Follow on-screen prompts** to map your remote's buttons
4. **Test input** by composing a short message

#### Multi-device Setup
1. **Connect multiple CC3200 units** via UART crossover cable
2. **Set unique device IDs** in configuration (Device 1, Device 2, etc.)
3. **Test communication** by sending messages between units

## 🎮 Usage

### Basic Text Messaging

#### Composing Messages
1. **Press numeric keys** to enter text (T9-style input)
   - Press `2` once for 'A', twice for 'B', three times for 'C'
   - Press `0` for space, `1` for punctuation marks
2. **Use LAST button** to backspace/delete characters
3. **Press MUTE** to send the completed message

#### Navigation
- **VOL+/VOL-**: Navigate through menus and message lists
- **CH+**: Switch between input modes (numbers/letters/symbols)
- **CH-**: Access main menu and settings

#### Receiving Messages
- **Incoming messages** appear automatically on screen
- **Notification sound** plays when new message arrives
- **Message history** stored and accessible via menu

### Advanced Features

#### Message Management
```c
// Access message functions via main menu
- View Inbox          // See all received messages
- View Sent Items      // Review sent messages  
- Clear All Messages   // Delete message history
- Device Settings      // Configure device ID and preferences
```

#### Multi-line Messages
- **Automatic word wrap** for long messages
- **Scrollable display** for messages exceeding screen height
- **Character counter** shows remaining space

#### Status Indicators
- **Signal strength**: IR reception quality indicator
- **Connection status**: Shows UART link status with other devices
- **Battery level**: Power status (if battery-powered)

## 📁 File Structure

```
├── main.c                  # Main application and IR decoder engine
├── Adafruit_GFX.c         # Graphics library implementation
├── Adafruit_GFX.h         # Graphics library header and font support
├── Adafruit_OLED.c        # OLED driver implementation
├── Adafruit_SSD1351.h     # SSD1351 controller definitions and commands
├── glcdfont.h             # Bitmap font data for text rendering
├── uart_if.c              # UART interface for inter-device communication
├── pin_mux_config.c       # Pin multiplexer configuration
├── pin_mux_config.h       # Pin configuration header
├── cc3200v1p32.cmd        # Linker command file for CC3200
├── .ccsproject            # Code Composer Studio project file
├── .cproject              # C/C++ project configuration
├── .project               # Eclipse project file
├── Debug/                 # Build output and object files
├── .settings/             # IDE configuration directory
├── .launches/             # Debug launch configurations
└── targetConfigs/         # Target device configurations
```

## 🔧 Customization

### Modify IR Protocol
```c
// Adjust timing parameters in main.c for different remotes
#define IR_BIT_DURATION     560    // Microseconds per bit
#define IR_HEADER_HIGH      9000   // Header pulse duration
#define IR_HEADER_LOW       4500   // Header space duration
#define IR_REPEAT_DELAY     40000  // Repeat signal timing
```

### Change Message Limits
```c
// Configure message system parameters
#define MAX_MESSAGE_LENGTH  128    // Characters per message
#define MAX_STORED_MESSAGES 10     // Message history size
#define DEVICE_ID          1       // Unique identifier (1-255)
```

### Customize Display Colors
```c
// Available colors in Adafruit_SSD1351.h
#define TEXT_COLOR         WHITE   // Message text color
#define BACKGROUND_COLOR   BLACK   // Screen background
#define MENU_HIGHLIGHT     BLUE    // Selected menu item
#define ERROR_COLOR        RED     // Error messages
#define SUCCESS_COLOR      GREEN   // Success notifications
```

### Adjust T9 Input Timing
```c
// Multi-tap input timing
#define MULTITAP_TIMEOUT   1000    // Milliseconds to wait for next key
#define REPEAT_DELAY       500     // Key repeat start delay
#define REPEAT_RATE        100     // Key repeat rate
```

## 🐛 Troubleshooting

### Common Issues

#### IR Remote Not Working
- **Check IR receiver**: Verify 5V/3.3V power and ground connections
- **Test with different remote**: Some remotes use different protocols
- **Adjust sensitivity**: Modify IR threshold values in code
- **Check pin assignment**: Ensure IR data pin matches configuration

#### OLED Display Issues
- **Verify SPI connections**: Check MOSI, CLK, CS, DC, RST pins
- **Power supply check**: Ensure stable 3.3V supply
- **Initialization problems**: Check SSD1351 startup sequence
- **Garbled display**: Verify SPI clock speed and signal integrity

#### UART Communication Failure
- **Check wiring**: TX->RX, RX->TX crossover between devices
- **Baud rate mismatch**: Verify both devices use same baud rate (115200)
- **Flow control**: Ensure proper handshaking implementation
- **Cable length**: Keep UART cables short for reliable communication

#### Message Display Problems
- **Text rendering**: Check font data and character encoding
- **Memory issues**: Monitor stack usage during message processing
- **Timing problems**: Verify interrupt priorities and timing
- **Buffer overflow**: Check message length limits

### Performance Optimization

#### Response Time Improvements
```c
// Reduce IR processing delay
// Optimize OLED update frequency  
// Use interrupt-driven UART
// Implement message caching
```

#### Memory Usage
```c
// Current usage: ~4KB RAM
// Message buffer: ~2KB
// Display buffer: ~1KB
// Stack usage: ~1KB
```

## 📚 API Reference

### IR Decoder Functions
```c
void DecodeIR(void);                    // Main IR signal decoder
void ProcessIRCommand(uint32_t code);   // Handle decoded IR commands
bool IsValidIRSignal(void);             // Validate IR timing
```

### Message System Functions
```c
void ComposeMessage(char* text);        // Start message composition
void SendMessage(char* message);        // Transmit message via UART
void ReceiveMessage(void);              // Handle incoming messages
void DisplayMessageList(void);          // Show message history
```

### Display Functions
```c
void UpdateScreen(void);                // Refresh OLED display
void ShowMenu(MenuType menu);           // Display menu screens
void ShowMessage(char* text);           // Display single message
void ShowStatusBar(void);               // Update status indicators
```

### UART Communication Functions
```c
void InitUART(void);                    // Initialize UART interfaces
void SendPacket(MessagePacket* packet); // Send structured message
bool ReceivePacket(MessagePacket* packet); // Receive message packet
```

## 🤝 Contributing

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/new-protocol`
3. **Commit** changes: `git commit -am 'Add support for Sony IR protocol'`
4. **Push** to branch: `git push origin feature/new-protocol`
5. **Submit** a Pull Request

### Development Guidelines
- **Follow** existing code style and commenting conventions
- **Test** thoroughly on actual hardware before submitting
- **Update** documentation for new features and protocols
- **Consider** backward compatibility with existing message format

## 📄 License

This project is based on Adafruit libraries and follows their BSD license terms. All original code additions are provided under the same BSD license.

```
BSD License - See individual source files for full license text
Adafruit contributions: Copyright (c) Adafruit Industries
Project modifications: Open source contributions welcome
```

## 🙏 Acknowledgments

- **Adafruit Industries** - Graphics libraries and OLED driver foundation
- **Texas Instruments** - CC3200 SDK and comprehensive development tools
- **NEC Corporation** - IR protocol specification and documentation
- **Contributors** - Community improvements, bug fixes, and feature additions

---

**Project Status**: ✅ Active Development  
**Last Updated**: February 2024  
**Tested Platforms**: CC3200 LaunchPad, Custom CC3200 boards
**Compatible Remotes**: Most standard TV/STB remotes using NEC protocol 