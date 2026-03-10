# Technical Specification for Gauge

## Introduction
This document provides a comprehensive technical specification for the Gauge project. It includes all relevant sections, code examples, and technical details necessary for implementation.

## 1. Overview
The Gauge is a multi-functional tool that provides various measurements and outputs based on user interactions.

## 2. Code Examples
### 2.1 Arduino Implementation
```cpp
// Arduino code to initialize the gauge
#include <Wire.h>

void setup() {
  Serial.begin(9600);
  // Initialization code here
}

void loop() {
  // Main code here
}
```

### 2.2 Python GUI
```python
import tkinter as tk

class GaugeApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Gauge GUI")
        # Setup GUI elements here

    def run(self):
        self.root.mainloop()

if __name__ == '__main__':
    root = tk.Tk()
    app = GaugeApp(root)
    app.run()
```

## 3. EEPROM Maps
| Address | Value          | Description         |
|---------|----------------|---------------------|
| 0x00   | 0xFF           | Calibration Data     |
| 0x01   | 0x01           | Configuration Flag   |

## 4. Serial Protocol
The protocol for serial communication between the Gauge and external devices follows these specifications:
- Baud Rate: 9600
- Data Bits: 8
- Stop Bits: 1
- Parity: None

## 5. Voltage Calculations
To ensure accurate readings, the following voltage calculations must be performed:
- Vout = Vin * (R2 / (R1 + R2))

## 6. Splash Screens
The splash screen should display the following details:
- Project Name
- Version Information
- Author Name

## 7. Firmware Upload
Firmware updates can be uploaded via the following process:
1. Connect the Gauge to the computer.
2. Use the Arduino IDE to upload the firmware.

## 8. Testing Plans
### 8.1 Unit Testing
Each module must be tested independently to ensure correct functionality.
### 8.2 Integration Testing
Test the interaction between various components to confirm they work as expected.
### 8.3 User Acceptance Testing
Involve end-users to validate the functionality of the Gauge in real-world scenarios.

## Conclusion
This technical specification outlines all necessary sections and details for the Gauge project, providing a clear guide for implementation and testing.

10. Custom Splash Screen Configuration
10.1 Memory Allocation
EEPROM Memory Map Update:

Code
Address Range    | Size    | Content
-----------------|---------|-----------------------------------------
0-255           | 256 B   | EXISTING DATA (odometer, display, etc.)
256-699         | 444 B   | CALIBRATION DATA
700-703         | 4 B     | SPLASH_CONFIG
                |         |   - Byte 0: startup_splash_id (0-255)
                |         |   - Byte 1: shutdown_splash_id (0-255)
                |         |   - Byte 2: custom_splash_flags
                |         |   - Byte 3: reserved
704-1215        | 512 B   | CUSTOM_SPLASH_1
1216-1727       | 512 B   | CUSTOM_SPLASH_2
1728-2239       | 512 B   | RESERVED (future expansion)
--------------------------------
TOTAL USED:     2,240 bytes
FREE EEPROM:    1,856 bytes
10.2 Splash Screen Enumeration
config_calibration.h - Add:

C++
// Splash screen IDs
enum SplashScreenID {
  SPLASH_FALCON_SCRIPT = 0,   // Built-in: "Falcon Script" logo (PROGMEM)
  SPLASH_302_CID = 1,          // Built-in: "302 CID" logo (PROGMEM)
  SPLASH_CUSTOM_1 = 2,         // User uploaded (EEPROM slot 1)
  SPLASH_CUSTOM_2 = 3,         // User uploaded (EEPROM slot 2)
  SPLASH_NONE = 255            // No splash screen (blank)
};

// Splash configuration
extern uint8_t startup_splash_id;
extern uint8_t shutdown_splash_id;
extern uint8_t custom_splash_flags;  // Bit 0: slot 1 valid, Bit 1: slot 2 valid

// EEPROM addresses for custom splash screens
#define SPLASH_CONFIG_ADDR 700
#define CUSTOM_SPLASH_1_ADDR 704
#define CUSTOM_SPLASH_2_ADDR 1216
#define CUSTOM_SPLASH_SIZE 512
config_calibration.cpp - Add:

C++
// Default splash configuration
uint8_t startup_splash_id = SPLASH_FALCON_SCRIPT;
uint8_t shutdown_splash_id = SPLASH_302_CID;
uint8_t custom_splash_flags = 0x00;  // No custom splashes loaded
10.3 Arduino Implementation
display.h - Add declarations
C++
// Splash screen functions
void displaySplashScreen(Adafruit_SSD1306 *display, uint8_t splash_id);
void loadCustomSplashFromEEPROM(uint8_t* buffer, uint8_t slot);
void saveCustomSplashToEEPROM(const uint8_t* buffer, uint8_t slot);
bool validateCustomSplash(uint8_t slot);
display.cpp - Implementation
C++
/**
 * displaySplashScreen - Display splash screen by ID
 * 
 * @param display - Pointer to SSD1306 display object
 * @param splash_id - Splash screen ID (enum SplashScreenID)
 */
void displaySplashScreen(Adafruit_SSD1306 *display, uint8_t splash_id) {
  uint8_t buffer[CUSTOM_SPLASH_SIZE];
  
  switch(splash_id) {
    case SPLASH_FALCON_SCRIPT:
      // Load built-in "Falcon Script" from PROGMEM
      memcpy_P(buffer, falcon_script_bitmap, CUSTOM_SPLASH_SIZE);
      break;
    
    case SPLASH_302_CID:
      // Load built-in "302 CID" from PROGMEM
      memcpy_P(buffer, cid302_bitmap, CUSTOM_SPLASH_SIZE);
      break;
    
    case SPLASH_CUSTOM_1:
      // Check if custom splash 1 is valid
      if (custom_splash_flags & 0x01) {
        loadCustomSplashFromEEPROM(buffer, 0);
      } else {
        // Invalid/empty slot, show blank
        display->clearDisplay();
        display->display();
        return;
      }
      break;
    
    case SPLASH_CUSTOM_2:
      // Check if custom splash 2 is valid
      if (custom_splash_flags & 0x02) {
        loadCustomSplashFromEEPROM(buffer, 1);
      } else {
        display->clearDisplay();
        display->display();
        return;
      }
      break;
    
    case SPLASH_NONE:
      // No splash screen, just clear display
      display->clearDisplay();
      display->display();
      return;
    
    default:
      // Unknown ID, default to blank
      display->clearDisplay();
      display->display();
      return;
  }
  
  // Render bitmap to display
  display->clearDisplay();
  display->drawBitmap(0, 0, buffer, 128, 32, WHITE);
  display->display();
}

/**
 * loadCustomSplashFromEEPROM - Load custom splash from EEPROM slot
 * 
 * @param buffer - 512-byte buffer to load into
 * @param slot - Slot number (0 or 1)
 */
void loadCustomSplashFromEEPROM(uint8_t* buffer, uint8_t slot) {
  uint16_t addr = CUSTOM_SPLASH_1_ADDR + (slot * CUSTOM_SPLASH_SIZE);
  
  for (uint16_t i = 0; i < CUSTOM_SPLASH_SIZE; i++) {
    buffer[i] = EEPROM.read(addr + i);
  }
}

/**
 * saveCustomSplashToEEPROM - Save custom splash to EEPROM slot
 * 
 * @param buffer - 512-byte bitmap data
 * @param slot - Slot number (0 or 1)
 */
void saveCustomSplashToEEPROM(const uint8_t* buffer, uint8_t slot) {
  uint16_t addr = CUSTOM_SPLASH_1_ADDR + (slot * CUSTOM_SPLASH_SIZE);
  
  // Use EEPROM.update() to minimize wear (only writes if value changed)
  for (uint16_t i = 0; i < CUSTOM_SPLASH_SIZE; i++) {
    EEPROM.update(addr + i, buffer[i]);
  }
  
  // Update custom splash flags to mark slot as valid
  custom_splash_flags |= (0x01 << slot);
  EEPROM.update(SPLASH_CONFIG_ADDR + 2, custom_splash_flags);
}

/**
 * validateCustomSplash - Check if custom splash slot contains valid data
 * 
 * @param slot - Slot number (0 or 1)
 * @return true if slot contains valid bitmap
 */
bool validateCustomSplash(uint8_t slot) {
  // Check if slot is marked as valid in flags
  return (custom_splash_flags & (0x01 << slot)) != 0;
}
gauge_V4.ino - Modified startup
C++
void setup() {
  // ... existing setup code ...
  
  // ===== DISPLAY INITIALIZATION =====
  display1.begin(SSD1306_SWITCHCAPVCC, 0, true, true, OLED_SPI_CLOCK);
  display2.begin(SSD1306_SWITCHCAPVCC, 0, true, true, OLED_SPI_CLOCK);
  
  // Load splash configuration from EEPROM
  startup_splash_id = EEPROM.read(SPLASH_CONFIG_ADDR);
  shutdown_splash_id = EEPROM.read(SPLASH_CONFIG_ADDR + 1);
  custom_splash_flags = EEPROM.read(SPLASH_CONFIG_ADDR + 2);
  
  // Display startup splash screens
  displaySplashScreen(&display1, startup_splash_id);
  displaySplashScreen(&display2, startup_splash_id);  // Or different ID if desired
  
  // ... rest of setup ...
}
utilities.cpp - Modified shutdown
C++
void shutdown() {
  // ... existing shutdown code ...
  
  // Display shutdown splash screens
  displaySplashScreen(&display1, shutdown_splash_id);
  displaySplashScreen(&display2, shutdown_splash_id);
  
  // Brief delay to show splash before power cut
  delay(1000);
  
  // Cut power
  digitalWrite(PWR_PIN, LOW);
}
10.4 Serial Protocol - Splash Screen Commands
Command Set
Test Splash Display (without saving):

Code
PC  → Arduino: TEST_SPLASH 1
              (Display custom splash slot 1 temporarily)
Arduino → PC:  OK
Upload Custom Splash:

Code
PC  → Arduino: UPLOAD_SPLASH_START 0 512
              (slot=0 for CUSTOM_SPLASH_1, size=512 bytes)
Arduino → PC:  READY

PC  → Arduino: SPLASH_DATA 0040FF00AA55... (hex-encoded chunk)
              (Send in 64-byte chunks, 8 total chunks)
Arduino → PC:  OK

PC  → Arduino: SPLASH_DATA 1122334455... (next chunk)
Arduino → PC:  OK

... (repeat for all 8 chunks)

PC  → Arduino: UPLOAD_SPLASH_END
Arduino → PC:  OK:CRC=0xABCD1234
(Bitmap now saved to EEPROM, slot marked valid)
Set Startup/Shutdown Splash:

Code
PC  → Arduino: SET_STARTUP_SPLASH 2
              (Use SPLASH_CUSTOM_1)
Arduino → PC:  OK

PC  → Arduino: SET_SHUTDOWN_SPLASH 3
              (Use SPLASH_CUSTOM_2)
Arduino → PC:  OK

PC  → Arduino: GET_SPLASH_CONFIG
Arduino → PC:  STARTUP_SPLASH=2
Arduino → PC:  SHUTDOWN_SPLASH=3
Arduino → PC:  CUSTOM_FLAGS=0x03
Clear Custom Splash Slot:

Code
PC  → Arduino: CLEAR_SPLASH 0
              (Clear CUSTOM_SPLASH_1)
Arduino → PC:  OK
serial_config.cpp - Add handlers
C++
void handleSerialConfig() {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  
  // ... existing command handlers ...
  
  else if (cmd.startsWith("UPLOAD_SPLASH_START ")) {
    handleSplashUploadStart(cmd.substring(20));
  }
  else if (cmd.startsWith("SPLASH_DATA ")) {
    handleSplashData(cmd.substring(12));
  }
  else if (cmd == "UPLOAD_SPLASH_END") {
    handleSplashUploadEnd();
  }
  else if (cmd.startsWith("SET_STARTUP_SPLASH ")) {
    startup_splash_id = cmd.substring(19).toInt();
    EEPROM.update(SPLASH_CONFIG_ADDR, startup_splash_id);
    Serial.println("OK");
  }
  else if (cmd.startsWith("SET_SHUTDOWN_SPLASH ")) {
    shutdown_splash_id = cmd.substring(20).toInt();
    EEPROM.update(SPLASH_CONFIG_ADDR + 1, shutdown_splash_id);
    Serial.println("OK");
  }
  else if (cmd == "GET_SPLASH_CONFIG") {
    Serial.print("STARTUP_SPLASH="); Serial.println(startup_splash_id);
    Serial.print("SHUTDOWN_SPLASH="); Serial.println(shutdown_splash_id);
    Serial.print("CUSTOM_FLAGS=0x"); Serial.println(custom_splash_flags, HEX);
  }
  else if (cmd.startsWith("CLEAR_SPLASH ")) {
    uint8_t slot = cmd.substring(13).toInt();
    if (slot < 2) {
      custom_splash_flags &= ~(0x01 << slot);
      EEPROM.update(SPLASH_CONFIG_ADDR + 2, custom_splash_flags);
      Serial.println("OK");
    } else {
      Serial.println("ERROR:INVALID_SLOT");
    }
  }
  else if (cmd.startsWith("TEST_SPLASH ")) {
    uint8_t splash_id = cmd.substring(12).toInt();
    displaySplashScreen(&display1, splash_id);
    Serial.println("OK");
  }
}

// Splash upload state machine
uint8_t upload_slot = 0;
uint16_t upload_bytes_received = 0;
uint8_t upload_buffer[512];

void handleSplashUploadStart(String params) {
  // Parse: "0 512" (slot, size)
  int spaceIdx = params.indexOf(' ');
  upload_slot = params.substring(0, spaceIdx).toInt();
  uint16_t size = params.substring(spaceIdx + 1).toInt();
  
  if (upload_slot > 1) {
    Serial.println("ERROR:INVALID_SLOT");
    return;
  }
  
  if (size != 512) {
    Serial.println("ERROR:INVALID_SIZE");
    return;
  }
  
  upload_bytes_received = 0;
  memset(upload_buffer, 0, 512);
  Serial.println("READY");
}

void handleSplashData(String hex_data) {
  // Convert hex string to bytes
  uint8_t chunk_size = hex_data.length() / 2;
  
  for (uint8_t i = 0; i < chunk_size; i++) {
    String byte_str = hex_data.substring(i*2, i*2 + 2);
    upload_buffer[upload_bytes_received++] = strtol(byte_str.c_str(), NULL, 16);
  }
  
  Serial.println("OK");
}

void handleSplashUploadEnd() {
  if (upload_bytes_received != 512) {
    Serial.print("ERROR:INCOMPLETE_UPLOAD:");
    Serial.println(upload_bytes_received);
    return;
  }
  
  // Save to EEPROM
  saveCustomSplashToEEPROM(upload_buffer, upload_slot);
  
  // Calculate CRC32 for verification
  uint32_t crc = calculateCRC32(upload_buffer, 512);
  
  Serial.print("OK:CRC=0x");
  Serial.println(crc, HEX);
}
10.5 Python Implementation - Image Conversion
Image Loader and Converter
splash_converter.py:

Python
"""
SSD1306 Splash Screen Converter
Converts images to 128x32 monochrome bitmap format for SSD1306 OLED displays
"""

from PIL import Image
import numpy as np

class SplashConverter:
    WIDTH = 128
    HEIGHT = 32
    SIZE_BYTES = 512  # 128 * 32 / 8
    
    def __init__(self):
        pass
    
    def load_and_convert(self, filepath):
        """
        Load image file and convert to SSD1306 format
        
        Args:
            filepath: Path to image file (.bmp, .png, .jpg, .gif supported)
        
        Returns:
            bytes: 512-byte array in SSD1306 vertical addressing format
        
        Raises:
            ValueError: If image validation fails
            FileNotFoundError: If file doesn't exist
        """
        # Load image
        try:
            img = Image.open(filepath)
        except FileNotFoundError:
            raise FileNotFoundError(f"Image file not found: {filepath}")
        except Exception as e:
            raise ValueError(f"Failed to load image: {e}")
        
        # Validate and convert
        img_converted = self._validate_and_convert(img)
        
        # Convert to SSD1306 byte array
        bitmap_bytes = self._convert_to_ssd1306_format(img_converted)
        
        return bitmap_bytes
    
    def _validate_and_convert(self, img):
        """
        Validate image dimensions and convert to 1-bit monochrome
        
        Args:
            img: PIL Image object
        
        Returns:
            PIL Image: 128x32 1-bit monochrome image
        
        Raises:
            ValueError: If validation fails
        """
        # Check if resize is needed
        if img.size != (self.WIDTH, self.HEIGHT):
            # Resize with high-quality resampling
            img = img.resize((self.WIDTH, self.HEIGHT), Image.Resampling.LANCZOS)
            print(f"Image resized from {img.size} to {self.WIDTH}x{self.HEIGHT}")
        
        # Convert to grayscale first
        if img.mode != 'L':
            img = img.convert('L')
        
        # Convert to 1-bit using Floyd-Steinberg dithering for best quality
        img_1bit = img.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
        
        return img_1bit
    
    def _convert_to_ssd1306_format(self, img):
        """
        Convert 1-bit PIL Image to SSD1306 vertical byte addressing format
        
        SSD1306 format:
        - Display is divided into 4 "pages" of 8 pixels tall
        - Each byte represents 8 vertical pixels
        - Bit 0 = top pixel, Bit 7 = bottom pixel of that byte
        - Bytes are arranged left-to-right, top-to-bottom by page
        
        Memory layout:
        Page 0: [byte 0-127]   (rows 0-7)
        Page 1: [byte 128-255] (rows 8-15)
        Page 2: [byte 256-383] (rows 16-23)
        Page 3: [byte 384-511] (rows 24-31)
        
        Args:
            img: 128x32 1-bit PIL Image
        
        Returns:
            bytes: 512-byte array
        """
        # Convert image to numpy array for easier manipulation
        img_array = np.array(img)
        
        # Initialize output buffer
        bitmap = bytearray(self.SIZE_BYTES)
        
        # Process by pages (4 pages of 8 pixels each)
        for page in range(4):  # Pages 0-3
            for x in range(self.WIDTH):  # Columns 0-127
                byte_val = 0
                
                for bit in range(8):  # 8 pixels per byte
                    y = page * 8 + bit  # Actual pixel row
                    
                    # Get pixel value (0 = black, 255 = white in 1-bit mode)
                    pixel = img_array[y, x]
                    
                    # SSD1306: 1 = white pixel, 0 = black pixel
                    if pixel > 0:  # White pixel
                        byte_val |= (1 << bit)
                
                # Calculate byte index in output array
                byte_index = page * self.WIDTH + x
                bitmap[byte_index] = byte_val
        
        return bytes(bitmap)
    
    def preview_ascii(self, bitmap_bytes):
        """
        Generate ASCII art preview of bitmap
        
        Args:
            bitmap_bytes: 512-byte SSD1306 format bitmap
        
        Returns:
            str: ASCII art representation
        """
        lines = []
        
        for page in range(4):
            for bit in range(8):
                line = ""
                for x in range(self.WIDTH):
                    byte_index = page * self.WIDTH + x
                    byte_val = bitmap_bytes[byte_index]
                    
                    # Check if bit is set
                    if byte_val & (1 << bit):
                        line += "█"  # White pixel
                    else:
                        line += " "  # Black pixel
                
                lines.append(line)
        
        return "\n".join(lines)
    
    def save_as_c_array(self, bitmap_bytes, output_path, array_name="custom_splash"):
        """
        Save bitmap as C array for embedding in Arduino code
        
        Args:
            bitmap_bytes: 512-byte bitmap
            output_path: Path to save .h file
            array_name: Name of C array
        """
        with open(output_path, 'w') as f:
            f.write(f"// Auto-generated splash screen bitmap\n")
            f.write(f"// 128x32 monochrome, SSD1306 format\n\n")
            f.write(f"const uint8_t {array_name}[] PROGMEM = {{\n")
            
            for i in range(0, len(bitmap_bytes), 16):
                chunk = bitmap_bytes[i:i+16]
                hex_str = ", ".join([f"0x{b:02X}" for b in chunk])
                f.write(f"  {hex_str},\n")
            
            f.write("};\n")


# Example usage
if __name__ == "__main__":
    converter = SplashConverter()
    
    # Load and convert image
    bitmap = converter.load_and_convert("my_logo.png")
    
    # Preview in terminal
    print(converter.preview_ascii(bitmap))
    
    # Save as C header (for PROGMEM storage)
    converter.save_as_c_array(bitmap, "my_splash.h", "my_logo_bitmap")
GUI Integration - Serial Upload
config_tool.py:

Python
import serial
import time
from splash_converter import SplashConverter

class SplashUploader:
    CHUNK_SIZE = 64  # Send 64 bytes per command
    
    def __init__(self, serial_port):
        self.serial = serial_port
        self.converter = SplashConverter()
    
    def upload_splash(self, image_path, slot, progress_callback=None):
        """
        Upload custom splash screen to Arduino
        
        Args:
            image_path: Path to image file
            slot: 0 or 1 (CUSTOM_SPLASH_1 or CUSTOM_SPLASH_2)
            progress_callback: Function(percentage) for progress updates
        
        Returns:
            str: CRC checksum from Arduino
        
        Raises:
            ValueError: If validation fails
            RuntimeError: If upload fails
        """
        # Convert image to bitmap
        if progress_callback:
            progress_callback(0, "Converting image...")
        
        bitmap = self.converter.load_and_convert(image_path)
        
        if len(bitmap) != 512:
            raise ValueError(f"Bitmap conversion failed: got {len(bitmap)} bytes, expected 512")
        
        # Start upload
        if progress_callback:
            progress_callback(10, "Starting upload...")
        
        cmd = f"UPLOAD_SPLASH_START {slot} 512\n"
        self.serial.write(cmd.encode())
        
        response = self.serial.readline().decode().strip()
        if response != "READY":
            raise RuntimeError(f"Arduino not ready: {response}")
        
        # Send data in chunks
        total_chunks = (len(bitmap) + self.CHUNK_SIZE - 1) // self.CHUNK_SIZE
        
        for chunk_idx in range(total_chunks):
            start = chunk_idx * self.CHUNK_SIZE
            end = min(start + self.CHUNK_SIZE, len(bitmap))
            chunk = bitmap[start:end]
            
            # Convert to hex string
            hex_data = chunk.hex().upper()
            
            # Send chunk
            cmd = f"SPLASH_DATA {hex_data}\n"
            self.serial.write(cmd.encode())
            
            # Wait for ACK
            response = self.serial.readline().decode().strip()
            if response != "OK":
                raise RuntimeError(f"Upload failed at chunk {chunk_idx}: {response}")
            
            # Update progress
            if progress_callback:
                percentage = 10 + int((chunk_idx + 1) / total_chunks * 80)
                progress_callback(percentage, f"Uploading: {chunk_idx+1}/{total_chunks} chunks")
        
        # Finalize upload
        if progress_callback:
            progress_callback(95, "Finalizing...")
        
        self.serial.write(b"UPLOAD_SPLASH_END\n")
        response = self.serial.readline().decode().strip()
        
        if not response.startswith("OK:CRC="):
            raise RuntimeError(f"Upload finalization failed: {response}")
        
        crc = response.split("=")[1]
        
        if progress_callback:
            progress_callback(100, "Upload complete!")
        
        return crc
    
    def test_splash(self, splash_id):
        """
        Test display of splash screen without saving configuration
        
        Args:
            splash_id: 0-3 or 255 (SPLASH_NONE)
        """
        cmd = f"TEST_SPLASH {splash_id}\n"
        self.serial.write(cmd.encode())
        
        response = self.serial.readline().decode().strip()
        if response != "OK":
            raise RuntimeError(f"Test splash failed: {response}")
    
    def set_startup_splash(self, splash_id):
        """Set startup splash screen ID"""
        cmd = f"SET_STARTUP_SPLASH {splash_id}\n"
        self.serial.write(cmd.encode())
        
        response = self.serial.readline().decode().strip()
        if response != "OK":
            raise RuntimeError(f"Set startup splash failed: {response}")
    
    def set_shutdown_splash(self, splash_id):
        """Set shutdown splash screen ID"""
        cmd = f"SET_SHUTDOWN_SPLASH {splash_id}\n"
        self.serial.write(cmd.encode())
        
        response = self.serial.readline().decode().strip()
        if response != "OK":
            raise RuntimeError(f"Set shutdown splash failed: {response}")
    
    def get_splash_config(self):
        """
        Get current splash configuration
        
        Returns:
            dict: {'startup': int, 'shutdown': int, 'custom_flags': int}
        """
        self.serial.write(b"GET_SPLASH_CONFIG\n")
        
        config = {}
        for _ in range(3):
            line = self.serial.readline().decode().strip()
            if line.startswith("STARTUP_SPLASH="):
                config['startup'] = int(line.split("=")[1])
            elif line.startswith("SHUTDOWN_SPLASH="):
                config['shutdown'] = int(line.split("=")[1])
            elif line.startswith("CUSTOM_FLAGS="):
                config['custom_flags'] = int(line.split("=")[1], 16)
        
        return config
    
    def clear_splash(self, slot):
        """Clear custom splash slot"""
        cmd = f"CLEAR_SPLASH {slot}\n"
        self.serial.write(cmd.encode())
        
        response = self.serial.readline().decode().strip()
        if response != "OK":
            raise RuntimeError(f"Clear splash failed: {response}")
10.6 GUI - Splash Screen Tab
New Tab: Splash Screens

Python
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk

class SplashScreenTab(ttk.Frame):
    def __init__(self, parent, uploader):
        super().__init__(parent)
        self.uploader = uploader
        
        # Title
        title = ttk.Label(self, text="Splash Screen Configuration", 
                         font=('Arial', 14, 'bold'))
        title.pack(pady=10)
        
        # Startup/Shutdown selection
        selection_frame = ttk.LabelFrame(self, text="Splash Screen Assignment", 
                                        padding=10)
        selection_frame.pack(fill='x', padx=10, pady=5)
        
        # Startup splash
        ttk.Label(selection_frame, text="Startup Splash:").grid(row=0, column=0, 
                                                                sticky='w', pady=5)
        self.startup_var = tk.StringVar(value="0")
        startup_combo = ttk.Combobox(selection_frame, textvariable=self.startup_var,
                                     values=self._get_splash_options(), 
                                     state='readonly', width=30)
        startup_combo.grid(row=0, column=1, padx=10, pady=5)
        startup_combo.bind('<<ComboboxSelected>>', self._on_startup_changed)
        
        # Shutdown splash
        ttk.Label(selection_frame, text="Shutdown Splash:").grid(row=1, column=0, 
                                                                 sticky='w', pady=5)
        self.shutdown_var = tk.StringVar(value="1")
        shutdown_combo = ttk.Combobox(selection_frame, textvariable=self.shutdown_var,
                                      values=self._get_splash_options(), 
                                      state='readonly', width=30)
        shutdown_combo.grid(row=1, column=1, padx=10, pady=5)
        shutdown_combo.bind('<<ComboboxSelected>>', self._on_shutdown_changed)
        
        # Custom splash slots
        custom_frame = ttk.LabelFrame(self, text="Custom Splash Images", padding=10)
        custom_frame.pack(fill='both', expand=True, padx=10, pady=5)
        
        # Create two custom splash slot frames
        self._create_custom_slot(custom_frame, 0, "Custom Splash 1")
        self._create_custom_slot(custom_frame, 1, "Custom Splash 2")
        
        # Instructions
        info_frame = ttk.LabelFrame(self, text="Image Requirements", padding=10)
        info_frame.pack(fill='x', padx=10, pady=5)
        
        info_text = """
• Dimensions: 128 x 32 pixels (will be auto-resized if different)
• Format: Any common image format (.BMP, .PNG, .JPG, .GIF)
• Color: Will be auto-converted to 1-bit monochrome
• Size: Each image uses 512 bytes of EEPROM storage
• Maximum: 2 custom images
        """
        ttk.Label(info_frame, text=info_text, justify='left').pack()
    
    def _get_splash_options(self):
        return [
            "0 - Falcon Script (built-in)",
            "1 - 302 CID (built-in)",
            "2 - Custom Splash 1",
            "3 - Custom Splash 2",
            "255 - None (blank)"
        ]
    
    def _create_custom_slot(self, parent, slot, title):
        slot_frame = ttk.LabelFrame(parent, text=title, padding=10)
        slot_frame.pack(fill='x', pady=5)
        
        # Preview canvas (128x32 scaled 3x for visibility)
        preview_frame = ttk.Frame(slot_frame)
        preview_frame.pack(side='left', padx=10)
        
        canvas = tk.Canvas(preview_frame, width=128*3, height=32*3, 
                          bg='black', relief='sunken', bd=2)
        canvas.pack()
        
        # Store canvas reference
        if not hasattr(self, 'preview_canvases'):
            self.preview_canvases = {}
        self.preview_canvases[slot] = canvas
        
        # Buttons
        button_frame = ttk.Frame(slot_frame)
        button_frame.pack(side='right', padx=10)
        
        upload_btn = ttk.Button(button_frame, text="Upload Image...",
                               command=lambda: self._upload_image(slot))
        upload_btn.pack(pady=5)
        
        test_btn = ttk.Button(button_frame, text="Test on Display",
                             command=lambda: self._test_splash(slot + 2))
        test_btn.pack(pady=5)
        
        clear_btn = ttk.Button(button_frame, text="Clear Slot",
                              command=lambda: self._clear_slot(slot))
        clear_btn.pack(pady=5)
        
        # Status label
        status_label = ttk.Label(button_frame, text="Empty", foreground='gray')
        status_label.pack(pady=5)
        
        if not hasattr(self, 'status_labels'):
            self.status_labels = {}
        self.status_labels[slot] = status_label
    
    def _upload_image(self, slot):
        # File dialog
        filepath = filedialog.askopenfilename(
            title=f"Select Image for Custom Splash {slot + 1}",
            filetypes=[
                ("All Images", "*.bmp *.png *.jpg *.jpeg *.gif"),
                ("BMP files", "*.bmp"),
                ("PNG files", "*.png"),
                ("JPEG files", "*.jpg *.jpeg"),
                ("GIF files", "*.gif"),
                ("All files", "*.*")
            ]
        )
        
        if not filepath:
            return
        
        # Create progress dialog
        progress_window = tk.Toplevel(self)
        progress_window.title("Uploading Splash Screen")
        progress_window.geometry("400x150")
        progress_window.transient(self)
        progress_window.grab_set()
        
        ttk.Label(progress_window, text=f"Uploading to Custom Splash {slot + 1}",
                 font=('Arial', 10, 'bold')).pack(pady=10)
        
        progress_var = tk.StringVar(value="Preparing...")
        progress_label = ttk.Label(progress_window, textvariable=progress_var)
        progress_label.pack()
        
        progress_bar = ttk.Progressbar(progress_window, length=350, mode='determinate')
        progress_bar.pack(pady=10)
        
        def update_progress(percentage, message):
            progress_bar['value'] = percentage
            progress_var.set(message)
            progress_window.update()
        
        try:
            # Upload
            crc = self.uploader.upload_splash(filepath, slot, update_progress)
            
            # Update preview
            self._update_preview(slot, filepath)
            
            # Update status
            self.status_labels[slot].config(text=f"Loaded (CRC: {crc})", 
                                           foreground='green')
            
            progress_window.destroy()
            messagebox.showinfo("Success", 
                              f"Custom Splash {slot + 1} uploaded successfully!\nCRC: {crc}")
        
        except Exception as e:
            progress_window.destroy()
            messagebox.showerror("Upload Failed", str(e))
    
    def _update_preview(self, slot, filepath):
        """Update preview canvas with image"""
        try:
            # Load and convert image
            converter = SplashConverter()
            img = Image.open(filepath)
            
            # Resize and convert to 1-bit
            if img.size != (128, 32):
                img = img.resize((128, 32), Image.Resampling.LANCZOS)
            img = img.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
            
            # Scale 3x for visibility
            img_scaled = img.resize((128*3, 32*3), Image.Resampling.NEAREST)
            
            # Convert to PhotoImage
            photo = ImageTk.PhotoImage(img_scaled)
            
            # Update canvas
            canvas = self.preview_canvases[slot]
            canvas.delete('all')
            canvas.create_image(0, 0, anchor='nw', image=photo)
            
            # Keep reference to prevent garbage collection
            canvas.image = photo
        
        except Exception as e:
            print(f"Preview update failed: {e}")
    
    def _test_splash(self, splash_id):
        """Test splash on display without saving configuration"""
        try:
            self.uploader.test_splash(splash_id)
            messagebox.showinfo("Test Splash", 
                              f"Splash {splash_id} displayed on Arduino")
        except Exception as e:
            messagebox.showerror("Test Failed", str(e))
    
    def _clear_slot(self, slot):
        """Clear custom splash slot"""
        if messagebox.askyesno("Clear Slot", 
                              f"Clear Custom Splash {slot + 1}?"):
            try:
                self.uploader.clear_splash(slot)
                
                # Clear preview
                canvas = self.preview_canvases[slot]
                canvas.delete('all')
                
                # Update status
                self.status_labels[slot].config(text="Empty", foreground='gray')
                
                messagebox.showinfo("Success", f"Custom Splash {slot + 1} cleared")
            except Exception as e:
                messagebox.showerror("Clear Failed", str(e))
    
    def _on_startup_changed(self, event):
        """Handle startup splash selection change"""
        value = self.startup_var.get()
        splash_id = int(value.split(" ")[0])
        
        try:
            self.uploader.set_startup_splash(splash_id)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to set startup splash: {e}")
    
    def _on_shutdown_changed(self, event):
        """Handle shutdown splash selection change"""
        value = self.shutdown_var.get()
        splash_id = int(value.split(" ")[0])
        
        try:
            self.uploader.set_shutdown_splash(splash_id)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to set shutdown splash: {e}")
    
    def load_from_arduino(self):
        """Load current splash configuration from Arduino"""
        try:
            config = self.uploader.get_splash_config()
            
            self.startup_var.set(f"{config['startup']} - ...")
            self.shutdown_var.set(f"{config['shutdown']} - ...")
            
            # Update slot status based on custom_flags
            for slot in range(2):
                if config['custom_flags'] & (0x01 << slot):
                    self.status_labels[slot].config(text="Loaded", foreground='green')
                else:
                    self.status_labels[slot].config(text="Empty", foreground='gray')
        
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load splash config: {e}")
10.7 Updated EEPROM Save/Load Functions
eeprom_config.cpp - Add to save/load:

C++
void saveConfigToEEPROM() {
  uint16_t addr = CONFIG_START_ADDR;
  
  // ... existing scalar parameter saves ...
  
  // Save splash configuration
  EEPROM.update(SPLASH_CONFIG_ADDR, startup_splash_id);
  EEPROM.update(SPLASH_CONFIG_ADDR + 1, shutdown_splash_id);
  EEPROM.update(SPLASH_CONFIG_ADDR + 2, custom_splash_flags);
  EEPROM.update(SPLASH_CONFIG_ADDR + 3, 0);  // Reserved
  
  // Custom splash bitmaps are saved individually via saveCustomSplashToEEPROM()
  // Don't need to save here (already in EEPROM)
  
  // ... calculate and write checksum ...
}

bool loadConfigFromEEPROM() {
  uint16_t addr = CONFIG_START_ADDR;
  
  // ... existing parameter loads ...
  
  // Load splash configuration
  startup_splash_id = EEPROM.read(SPLASH_CONFIG_ADDR);
  shutdown_splash_id = EEPROM.read(SPLASH_CONFIG_ADDR + 1);
  custom_splash_flags = EEPROM.read(SPLASH_CONFIG_ADDR + 2);
  
  // Validate splash IDs
  if (startup_splash_id > 3 && startup_splash_id != 255) {
    startup_splash_id = SPLASH_FALCON_SCRIPT;  // Reset to default
  }
  if (shutdown_splash_id > 3 && shutdown_splash_id != 255) {
    shutdown_splash_id = SPLASH_302_CID;
  }
  
  // ... verify checksum and return ...
}
10.8 Implementation Checklist
Arduino Side
 Add splash screen enums to config_calibration.h
 Add EEPROM address definitions
 Add splash config variables
 Implement displaySplashScreen() in display.cpp
 Implement loadCustomSplashFromEEPROM()
 Implement saveCustomSplashToEEPROM()
 Implement validateCustomSplash()
 Add splash upload handlers to serial_config.cpp
 Add CRC32 calculation for bitmap validation
 Modify setup() to load and display startup splash
 Modify shutdown() to display shutdown splash
 Add splash config to EEPROM save/load functions
 Test EEPROM read/write performance (~1.7ms)
Python Side
 Create splash_converter.py module
 Implement SplashConverter class
 Implement PIL image loading
 Implement dimension validation and resize
 Implement 1-bit conversion with dithering
 Implement SSD1306 format conversion
 Implement ASCII preview (optional debug feature)
 Create SplashUploader class
 Implement serial upload protocol
 Implement progress callback
 Create Splash Screen GUI tab
 Implement file picker dialog
 Implement preview canvas (128x32 scaled 3x)
 Implement upload button handlers
 Implement test/clear slot buttons
 Implement startup/shutdown selection dropdowns
 Add error handling and validation
Testing
 Upload .bmp file (1-bit, 128x32) - should work perfectly
 Upload .png file (color, wrong size) - should resize and convert
 Upload .jpg file (color, correct size) - should convert
 Test both custom slots independently
 Test startup splash display on boot
 Test shutdown splash display on power-off
 Verify EEPROM persistence (power cycle)
 Test CRC validation
 Test "Clear Slot" functionality
 Test "Test on Display" functionality
 Verify no SRAM issues during upload
 Measure actual upload time (should be ~2-3 seconds)
 Test with complex images (gradients, photos, logos)
 Verify Floyd-Steinberg dithering quality
Summary of Custom Splash Screen Feature
✅ Memory Usage
Code
SRAM:      0 bytes (no impact)
EEPROM:    1,028 bytes (4 config + 2×512 images)
Flash:     ~3 KB (upload code + converter)
✅ Performance
Code
Upload time:     ~2-3 seconds (acceptable for config)
Load time:       ~1.7ms (negligible on startup)
Display time:    Instant (SSD1306 refresh)
✅ Features
2 user-uploadable custom splash screens
2 built-in splash screens (Falcon Script, 302 CID)
Separate startup and shutdown splash selection
Any image format supported (.BMP, .PNG, .JPG, .GIF)
Auto-resize and convert to 128x32 monochrome
High-quality Floyd-Steinberg dithering
Live preview in PC tool
Test on display before saving configuration
CRC validation for upload integrity
✅ User Workflow
User selects image file (any format, any size)
PC tool auto-converts to 128x32 monochrome
Preview shown in GUI
User clicks "Upload" → sends to Arduino via serial
Arduino saves to EEPROM, marks slot as valid
User selects startup/shutdown splash from dropdown
Configuration saved
Splash displays on next boot/shutdown
