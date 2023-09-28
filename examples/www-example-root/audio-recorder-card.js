// Dynamically load recorder.js from a URL, adjust accordingly
const recorderScript = document.createElement('script');
recorderScript.src = 'http://localhost:8123/local/recorder.js';
document.head.appendChild(recorderScript);

const SERVER_IP = "192.168.2.1";

recorderScript.onload = () => {
    class AudioRecorderCard extends HTMLElement {
        constructor() {
            super();
            this.attachShadow({ mode: 'open' });
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
                recordButton.addEventListener("pointerup", () => setTimeout(this.stopRecording.bind(this), 300));
                recordButton.addEventListener("touchend", () => setTimeout(this.stopRecording.bind(this), 300));
                recordButton.addEventListener("touchmove", this.stopRecording.bind(this));  // Handling unintentional drags
            }
        }

        startWebSocket() {
            // WebSocket initialization
            this.ws = new WebSocket(`ws://${SERVER_IP}:8089`);
            this.ws.onopen = function() {
                console.log("WebSocket connection opened");
            };
            this.ws.onerror = function(error) {
                console.error("WebSocket Error:", error);
            };
            this.ws.onclose = function() {
                console.log("WebSocket connection closed");
            };
        }

        sendAudioChunk(chunk) {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(chunk.buffer);
            }
        }

        async startRecording(event) {
            event.preventDefault();
        
            // Open WebSocket connection
            this.startWebSocket();

            this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            this.stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            let source = this.audioContext.createMediaStreamSource(this.stream);

            // Always create a new Recorder instance
            if (this.recorder) {
                this.recorder.clear();
            }
            this.recorder = new Recorder(source, { numChannels: 1 });
            this.recorder.record();
            this.content.querySelector("#recordButton").innerText = "Listening...";

            this.recorder.node.onaudioprocess = (e) => {
                if (!this.recorder.recording) return;

                // Get the audio buffer data
                let input = e.inputBuffer.getChannelData(0);
                let chunk = new Int16Array(input.length);
                for (let i = 0; i < input.length; i++) {
                    chunk[i] = Math.min(1, input[i]) * 0x7FFF;
                }
                this.sendAudioChunk(chunk);
            };
        }
        
        stopRecording(event = null) {
            if (event) {
                event.preventDefault();
            }
        
            if (this.recorder) {
                this.recorder.stop();
                this.stream.getTracks().forEach(track => track.stop());
            }
            
            this.content.querySelector("#recordButton").innerText = "Push and Hold to Talk";
            
            // Close the WebSocket after sending data
            if (this.ws) {
                this.ws.close();
                this.ws = null;
            }
            
            if (this.audioContext) {
                this.audioContext.close().then(() => {
                    this.audioContext = null;
                });
            }
        }

        setConfig(config) {
            this._config = config;
        }
    }

    customElements.define('audio-recorder-card', AudioRecorderCard);
};
