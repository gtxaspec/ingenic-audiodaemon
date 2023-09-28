# `web_client`: WebSocket Audio Streaming Client

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Usage](#usage)
- [Examples](#examples)
- [Requirements](#requirements)
- [Building](#building)
- [Sample Code](#sample)
- [Credits](#credits)

## Overview

`web_client` is an `iad` client designed for the capture and streaming of audio data from web browsers. By establishing a WebSocket server, it serves as a bridge, allowing web clients and backend systems to transfer audio data in real-time to the Ingenic Audio Daemon.

## Features

- **WebSocket Server**: Sets up a WebSocket server to handle incoming audio.
- **Audio Capture to iad**: Designed to capture audio from browsers and directly stream the data to the Ingenic Audio Daemon (`iad`).
- **IPv4 & IPv6 Support**: Dual-stack support ensures compatibility with both IPv4 and IPv6 network configurations.
- **Custom IP & Port Configuration**: By default, it listens on the local IP and port 8089, but offers flexibility with custom IP addresses and ports.
- **libwebsockets Integration**: Utilizes the robust `libwebsockets` library to manage WebSocket connections and data transfer.

## Usage

```bash
web_client [OPTIONS]
```

### Options:

- `-s`: Run in silent mode.
- `-i <ip_address>`: Specify a custom IP address for the WebSocket server (defaults to the local IP).
- `-p <port>`: Specify a custom port for the WebSocket server (default is 8089).
- `-d`: Enable debug mode
- `-h`: Display the help message and exit.

## Examples

1. **Default Execution**:
   Running `web_client` without arguments starts the WebSocket server on the local IP and the default port (8089).

   ```bash
   web_client
   ```

2. **Silent Mode**:
   For silent operation:

   ```bash
   web_client -s
   ```

3. **Custom IP & Port**:
   To use a specific IP and port:

   ```bash
   web_client -i 192.168.1.100 -p 9000
   ```

## Requirements

- **libwebsockets**: Ensure the `libwebsockets` library is properly installed and linked.

## Building

```
make deps
make web_client
```

## Sample Code

- An example demonstrating streaming of audio captured from a web browser to web_client using WebSockets. Refer to the directory `www-example-root/` for details.

## Credits

This project is inspired by the [simple-recorderjs-demo](https://github.com/addpipe/simple-recorderjs-demo/tree/master) from addpipe and uses the [Recorderjs](https://github.com/mattdiamond/Recorderjs) library from mattdiamond.
