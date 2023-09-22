# Ingenic Audio Daemon

The **Ingenic Audio Daemon** serves as an intermediary between the audio kernel modules and client applications, facilitating audio input and output operations.

## Prerequisites

Before running the audio daemon, ensure that the audio kernel modules are loaded. The daemon sets up the `IMP_AI` and `IMP_AO` devices, so it's essential for these modules to be initialized first.

## Running the Daemon

To run the audio daemon:

```
./audio_daemon
```

## Using the Audio Client

The audio client provides various functionalities, from playing audio to recording it.

### Usage:

```
./audio_client [-f <audio_file_path>] [-s] [-r <audio_output_file_path>]
```

#### Options:

- `-f`: Play an audio file specified by `<audio_file_path>`.
- `-s`: Read audio from the standard input (`stdin`).
- `-r`: Record audio and save it to a file specified by `<audio_output_file_path>`.

For example, if you want to play a specific audio file, you can use:

```
./audio_client -f path_to_your_audio_file.wav
```
