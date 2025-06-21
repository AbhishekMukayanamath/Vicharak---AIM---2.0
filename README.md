# Vicharak---AIM---2.0
this project includes ESP Dev-Kit-V1

ESP32(Dev Kit V1) HTTPS File Downloader with Speed Logging
1. Introduction
This project demonstrates how to perform a secure HTTPS file download on an ESP32 microcontroller using the ESP-IDF framework. It also measures and logs download and write speeds separately to assess the system's performance. The downloaded file is saved to the SPIFFS (SPI Flash File System) on the ESP32.
2. Key Features
- Wi-Fi initialization and connection handling
- HTTPS client communication using esp_http_client
- File system mounting using SPIFFS
- Real-time download and write speed calculation

3. System Workflow
The firmware initializes NVS, mounts SPIFFS, and establishes a Wi-Fi connection. Once connected, it creates an HTTPS client and initiates a connection to the specified file URL. The file is downloaded in chunks and written to the flash memory. During each iteration, it records the time taken to read from the network and write to SPIFFS, calculating both download and write speeds independently.
4. Code Structure
• Buffer Handling:
  The file is downloaded in chunks using a 1024-byte buffer. This chunked approach helps manage memory efficiently.
• HTTP Client Handling:
  The HTTP client is configured with SSL support using esp_http_client. The connection is opened using esp_http_client_open, and headers are fetched to retrieve content length.
• Timing and Speed Calculation:
  esp_timer_get_time() is used to record precise timestamps. Separate timers measure download and write durations for each chunk, allowing average speeds to be computed and logged.
5. Performance Metrics
The log output provides insight into the system's speed capabilities. For each test run, it prints:
- Total downloaded bytes
- Total written bytes
- Download speed in KB/s
- Write speed in KB/s

6. Use Case & Benchmark
This implementation is ideal for scenarios where large firmware or data files need to be retrieved from remote servers securely. It is also used to benchmark the ESP32's networking and file I/O performance. The target performance goal is a combined speed (download + write) of at least 400 KB/s.
7. Conclusion
This project offers a practical template for integrating HTTPS download with ESP32 systems. It is modular and extensible, suitable for production-level applications where monitoring and achieving performance targets are critical.


