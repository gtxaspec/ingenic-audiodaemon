# Ingenic Audio Daemon

The **Ingenic Audio Daemon** (iad) serves as an intermediary between the audio kernel modules and client applications, facilitating audio input and output operations on Ingenic hardware.

- **iad (Ingenic Audio Daemon)**: A background process for handling audio on Ingenic devices.
  - **iac (Ingenic Audio Client)**: A client-side utility to interact with the audio daemon.
  - **audioplay**: A standalone audio player for Ingenic devices.
  - **wc-console**: A client utility that establishes a WebSocket server, enabling the capture and streaming of audio data from web browsers.

---

## Prerequisites

- T20/T31/T40 Devices: Audio Input & Playback tested on T20/T31.  If you have a T40 device, feedback is welcome.
- Before running the audio daemon, ensure that the audio kernel modules are loaded.
- The daemon sets up the `IMP_AI` and `IMP_AO` devices, so it's essential for these modules to be initialized first.

---

## Features

- Low latency
- AI supports multiple clients
- AO supports queued playback from multiple sources

---

## Compiling

### Prerequisites

Before you begin, ensure you have:

- A Linux-based operating system.
- The required cross-compilation tools for Ingenic devices.
- Git (for versioning support in the build system).

### Directory Structure

- `src/`: Contains the source code for the tools. Each tool has its own subdirectory.
- `lib/`: Contains necessary libraries.
- `include/`: Contains header files.
- `build/`: Contains object files and binaries after compilation.
- `config/`: Contains configuration files and templates.

### Compiling the Tools

1. **Prepare the Environment**: Ensure you have set up the cross-compilation environment for Ingenic devices.

2. **Clone the Repository**

3. **Compile All Tools**: `make`
This will compile the audio daemon (iad), audio client (iac), and the standalone audio player (audioplay)

4. **Compile Individual Tools**:
If you only need to compile one of the tools, you can do so individually:
```
make iad        # For the audio daemon
make iac        # For the audio client
make audioplay  # For the standalone audio player
make wc-console # For the websocket server, run `make deps` to build dependencies, 
                # then `make wc-console`.
```

5. **Clean the Build**:
If you need to clean up the compiled objects and binaries:
`make clean`
For a deeper clean (removes the compiled binaries as well):
`make distclean`

---

## Running the Daemon

To run the audio daemon:

```
./iad [-d <AI|AO>]
```

#### Options:

- `-d`: Disable Audio Input or Output by `<AI|AO>`.

---

## Using the Audio Client

The audio client provides various functionalities, from playing audio to recording it.  Audio client functionality can be integrated into your own program to interface with the daemon if desired.

### Usage:

```
./iac [-f <audio_file_path>] [-s] [-r <audio_output_file_path>] [-o]
```

#### Options:

- `-f`: AO - Play an audio file specified by `<audio_file_path>`.
- `-s`: AO - Read audio from the standard input (`stdin`).
- `-r`: AI - Record audio and save it to a file specified by `<audio_output_file_path>`.
- `-o`: AI - Output audio to the standard output (`stdout`).

For example, if you want to play a specific audio file, you can use:

```
./iac -f path_to_your_audio_file.wav
```

#### stdin examples:

```
ffmpeg -re -i https://wpr-ice.streamguys1.com/wpr-ideas-mp3-64 -af volume=-15dB  -acodec pcm_s16le -f s16le -ac 1 -ar 48000 - | ./iac -s
ffmpeg -f s16le -ar 16000 -ac 1 -i test_file.pcm -acodec pcm_s16le -f s16le -ac 1 -ar 48000 - | ./iac -s
```

#### send audio from laptop to device:

on device:

```
nc -l -p 8081 | ffmpeg -f s16le -ar 48000 -ac 1 -i - -af volume=-15dB -f s16le -ar 48000 -ac 1 - | ./iac -s
```
on laptop:

```
ffmpeg -f alsa -ac 1 -i default -f s16le -ar 48000 -ac 1 - | ffmpeg -f s16le -i - -f s16le - | nc 192.168.2.2 8081
```

Latency is decent!

Note: Set the sample rate on the ffmpeg command line to match your settings.

---

#### todo:

- on the fly config changes via webui  
- output AI and AO parameters to logcat, add switch to log to logcat if desired  
- stdout logging that gives debug output upon initing AI and AO  
