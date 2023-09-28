// Dynamically load recorder.js from a URL, adjust accordingly
const recorderScript = document.createElement('script');
recorderScript.src = 'http://192.168.1.2:3333/library/recorder.js';
document.head.appendChild(recorderScript);

recorderScript.onload = () => {
    class AudioRecorderCard extends HTMLElement {
        constructor() {
            super();
            this.attachShadow({ mode: 'open' });
            this.audioChunks = [];
        }

        set hass(hass) {
            if (!this.content) {
                const card = document.createElement('ha-card');
                card.header = 'Two-Way Audio';

                const style = document.createElement('style');
                style.textContent = `
                    ha-card {
                        --ha-card-header-font-size: 20px;
                    }
                
                    #recordButton {
                        font-size: 22px;
                        padding: 15px 30px;
                        cursor: pointer;
                    }
                `;
                card.appendChild(style);
                
                
                this.content = document.createElement('div');
                this.content.className = 'card-content';
                
                const button = document.createElement('button');
                button.id = 'recordButton';
                button.innerText = 'Push and Hold to Talk';
                this.content.appendChild(button);

                card.appendChild(this.content);
                this.shadowRoot.appendChild(card);

                const recordButton = this.content.querySelector("#recordButton");
                recordButton.addEventListener("pointerdown", this.startRecording.bind(this));
                recordButton.addEventListener("pointerup", this.stopRecording.bind(this));
                recordButton.addEventListener("touchend", this.stopRecording.bind(this));   // Handling touchend
                recordButton.addEventListener("touchmove", this.stopRecording.bind(this));  // Handling unintentional drags
            }
        }

        async startRecording(event) {
            event.preventDefault();
        
            if (this.isRecording) return; // Prevent starting multiple recordings
            this.isRecording = true;
        
            // Always create a new AudioContext and Recorder instance
            this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            this.stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            let source = this.audioContext.createMediaStreamSource(this.stream);
            this.recorder = new Recorder(source, { numChannels: 1 });
        
            this.audioChunks = [];
            this.recorder.record();
            this.content.querySelector("#recordButton").innerText = "Listening...";
        }
        
        stopRecording(event) {
            event.preventDefault();
        
            if (!this.isRecording) return; // Prevent stopping multiple times
            this.isRecording = false;
        
            if (this.recorder) {
                this.recorder.stop();
                this.recorder.exportWAV(this.sendDataToServer.bind(this));
                this.recorder.clear();
                this.stream.getTracks().forEach(track => track.stop());
            }
        
            this.content.querySelector("#recordButton").innerText = "Push and Hold to Talk";
        }

        sendDataToServer(blob) {
            const formData = new FormData();
            formData.append('audio', blob, 'audio.wav');

            //Adjust the url to match the server address
            const serverUrl = `http://192.168.1.2:3333/upload`;

            fetch(serverUrl, {
                method: "POST",
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                console.log(data);
            })
            .catch(error => {
                console.error("There was an error uploading the audio:", error);
            });
        }

        setConfig(config) {
            this._config = config;
        }
    }

    customElements.define('audio-recorder-card', AudioRecorderCard);
};
