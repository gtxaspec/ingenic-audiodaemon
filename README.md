# Ingenic Audio Daemon

The **Ingenic Audio Daemon** serves as an intermediary between the audio kernel modules and client applications, facilitating audio input and output operations.

## Prerequisites

T20/31/T40: Audio Input & Playback tested on T20/T31.  Before running the audio daemon, ensure that the audio kernel modules are loaded. The daemon sets up the `IMP_AI` and `IMP_AO` devices, so it's essential for these modules to be initialized first.

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
- `-o`: Output audio to the standard output (`stdout`).

For example, if you want to play a specific audio file, you can use:

```
./audio_client -f path_to_your_audio_file.wav
```

#### stdin examples:

```
ffmpeg -re -i https://wpr-ice.streamguys1.com/wpr-ideas-mp3-64 -af volume=-15dB  -acodec pcm_s16le -f s16le -ac 1 -ar 48000 - | ./audio_client -s
ffmpeg -f s16le -ar 16000 -ac 1 -i test_file.pcm -acodec pcm_s16le -f s16le -ac 1 -ar 48000 - | ./audio_client -s
```

#### send audio from laptop to device:

on device:

```
nc -l -p 8081 | ffmpeg -f s16le -ar 48000 -ac 1 -i - -af volume=-15dB -f s16le -ar 48000 -ac 1 - | ./audio_client -s
```
on laptop:

```
ffmpeg -f alsa -ac 1 -i default -f s16le -ar 48000 -ac 1 - | ffmpeg -f s16le -i - -f s16le - | nc 192.168.2.2 8081
```

Latency is decent!

Note: Set the sample rate on the ffmpeg command line to match your settings.


#### todo:

json config file for all variables, including AI and AO device IDs, samplerate, etc, all AI and AO options should be configurable
on the fly config changes via webui
output AI and AO parameters to logcat, switch some logging to logcat
add option to disable AO or AI on run
