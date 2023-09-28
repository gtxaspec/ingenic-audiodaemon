const SERVER_IP = "192.168.2.1";

let audioContext;
let recorder;
let recordButton = document.getElementById("recordButton");
let stream;
let ws;

function startWebSocket() {
    // WebSocket initialization
    ws = new WebSocket(`ws://${SERVER_IP}:8089`);
    ws.onopen = function() {
        console.log("WebSocket connection opened");
    };
    ws.onerror = function(error) {
        console.error("WebSocket Error:", error);
    };
    ws.onclose = function() {
        console.log("WebSocket connection closed");
    };
}

function sendAudioChunk(chunk) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(chunk.buffer);
    }
}

function startRecording() {
    console.log("Attempting to start recording...");
    
    // Open WebSocket connection
    startWebSocket();

    // Always create a new AudioContext
    audioContext = new (window.AudioContext || window.webkitAudioContext)();

    navigator.mediaDevices.getUserMedia({ audio: true })
        .then(mediaStream => {
            stream = mediaStream;  // Store the media stream
            let source = audioContext.createMediaStreamSource(mediaStream);

            // Always create a new Recorder instance
            recorder = new Recorder(source, { numChannels: 1 });
            recorder.record();  // Start recording
            recordButton.innerText = ". . . Recording . . .";
            console.log("Recording started.");

            recorder.node.onaudioprocess = function (e) {
                if (!recorder.recording) return;

                // Get the audio buffer data
                let input = e.inputBuffer.getChannelData(0); // assuming mono audio
                let chunk = new Int16Array(input.length);
                for (let i = 0; i < input.length; i++) {
                    chunk[i] = Math.min(1, input[i]) * 0x7FFF;
                }
                sendAudioChunk(chunk);
            };
        })
        .catch(err => {
            console.error("Error accessing the microphone:", err);
        });
}

function stopRecording() {
    console.log("Attempting to stop recording...");
    recorder.stop();
    recordButton.innerText = "Push and Hold to Record";

    // Disconnect and stop the MediaStream tracks
    stream.getTracks().forEach(track => track.stop());

    // Close the AudioContext
    audioContext.close();

    // Close the WebSocket connection
    if (ws) {
        ws.close();
        ws = null;
    }

    console.log("Recording stopped.");
}

recordButton.addEventListener("mousedown", startRecording);
recordButton.addEventListener("mouseup", stopRecording);
