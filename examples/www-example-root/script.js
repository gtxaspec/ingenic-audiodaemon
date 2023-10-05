const SERVER_IP = "192.158.2.1";

let audioContext;
let recorder;
let recordButton = document.getElementById("recordButton");
let sampleRateSelector = document.getElementById("sampleRateSelector");
let stream;
let ws;

function resample(data, sourceSampleRate, targetSampleRate) {
    if (sourceSampleRate === targetSampleRate) {
        return data;
    }

    var ratio = sourceSampleRate / targetSampleRate;
    var newData = new Float32Array(Math.round(data.length / ratio));

    var offsetResult = 0;
    var offsetSource = 0;

    while (offsetSource < data.length) {
        newData[offsetResult] = data[Math.round(offsetSource)];
        offsetResult++;
        offsetSource += ratio;
    }

    return newData;
}

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

    // Get selected sample rate
    let selectedSampleRate = parseInt(sampleRateSelector.value);

    // Always create a new AudioContext
    audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: selectedSampleRate });

    // Always create a new AudioContext
    audioContext = new (window.AudioContext || window.webkitAudioContext)();

    navigator.mediaDevices.getUserMedia({ audio: true })
        .then(mediaStream => {
            stream = mediaStream;  // Store the media stream
            let source = audioContext.createMediaStreamSource(mediaStream);

            // Always create a new Recorder instance
            recorder = new Recorder(source, { numChannels: 1, targetSampleRate: selectedSampleRate });
            recorder.record();  // Start recording
            recordButton.innerText = ". . . Recording . . .";
            console.log("Recording started.");

recorder.node.onaudioprocess = function(e) {
    if (!recorder.recording) return;

    // Get the audio buffer data
    let input = e.inputBuffer.getChannelData(0); // assuming mono audio
    let resampledInput = resample(input, audioContext.sampleRate, recorder.config.targetSampleRate);

    let chunk = new Int16Array(resampledInput.length);
    for (let i = 0; i < resampledInput.length; i++) {
        chunk[i] = Math.min(1, resampledInput[i]) * 0x7FFF;
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

//Prevent context menu on mobile from appearing on button hold
recordButton.addEventListener("contextmenu", function(event) {
    event.preventDefault();
});

recordButton.addEventListener("mousedown", startRecording);
recordButton.addEventListener("mouseup", function() {
    setTimeout(stopRecording, 300);
});

recordButton.addEventListener("touchstart", startRecording);
recordButton.addEventListener("touchend", function() {
    setTimeout(stopRecording, 300);
});
