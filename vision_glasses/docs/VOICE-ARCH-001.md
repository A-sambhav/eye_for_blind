# Voice Assistant Architecture Document

**Document ID:** VOICE-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Audio/AI Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Voice Assistant Overview
3. Speech Recognition
4. Wake Word Detection
5. Noise Cancellation
6. Intent Recognition
7. Command Parsing
8. Response Generation
9. Text-to-Speech
10. Conversation Flow
11. Emergency Commands
12. Navigation Commands
13. Reading Commands
14. Reminder Commands
15. Accessibility Features
16. Dialogue Flow Diagrams
17. Sequence Diagrams
18. Architecture Diagrams

---

## 1. Executive Summary

The Voice Assistant is the primary interface for the smart glasses. All user interaction is voice-driven, with haptic feedback as a secondary modality. The system uses a wake word ("Hey Glass") to activate, processes commands through Edge Impulse keyword spotting, parses intents, and responds via TTS through the bone conduction speaker. The architecture prioritizes low latency (< 200 ms response) and high accuracy even in noisy environments.

---

## 2. Voice Assistant Overview

### 2.1 Block Diagram

```
+---------------------------------------------------------------------+
|                       VOICE ASSISTANT SYSTEM                         |
+---------------------------------------------------------------------+
|                                                                     |
|  MICROPHONE (MEMS SPH0645, I2S, 48 kHz, 16-bit mono)               |
|       │                                                             |
|       v                                                             |
|  ┌──────────────────────────────────────────────────────────────────┐|
|  |                   AUDIO PRE-PROCESSOR                            ||
|  |  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  ||
|  |  │ DC Removal   │  │ Pre-emphasis │  │ Frame Buffer         │  ||
|  |  │ (HPF 20 Hz)  │  │ (1-0.97z⁻¹)  │  │ (32 ms = 1536 samp) │  ||
|  |  └──────────────┘  └──────────────┘  └──────────────────────┘  ||
|  └──────────────────────────┬───────────────────────────────────────┘|
|                             │                                        |
|                             v                                        |
|  ┌──────────────────────────────────────────────────────────────────┐|
|  |                VOICE ACTIVITY DETECTOR (VAD)                     ||
|  |  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  ||
|  |  │ Energy       │  │ Zero-crossing│  │ Adaptive threshold   │  ||
|  |  │ (frame RMS)  │  │ Rate (ZCR)   │  │ (noise floor + 12dB) │  ||
|  |  └──────────────┘  └──────────────┘  └──────────────────────┘  ||
|  └──────────────────────────┬───────────────────────────────────────┘|
|                             │                                        |
|              ┌──────────────┴──────────────┐                         |
|              v                              v                        |
|  ┌─────────────────────┐    ┌─────────────────────────┐            |
|  │ WAKE WORD DETECTOR  │    │ NOISE CANCELLATION       │            |
|  │ Edge Impulse DS-CNN │    │ - Wiener filter          │            |
|  │ "Hey Glass"         │    │ - Spectral subtraction   │            |
|  │ On every VAD frame  │    │   (non-stationary noise) │            |
|  └──────────┬──────────┘    └────────────┬─────────────┘            |
|             │                            │                          |
|             │ (wake word detected)        │ (always running)         |
|             v                            │                          |
|  ┌─────────────────────┐                 │                          |
|  │ COMMAND RECOGNIZER  │◀────────────────┘                          |
|  │ Edge Impulse DS-CNN │                                            |
|  │ 26 classes (25 cmds)│                                            |
|  │ On 500ms audio      │                                            |
|  └──────────┬──────────┘                                            |
|             │                                                        |
|             v                                                        |
|  ┌─────────────────────┐                                            |
|  │ INTENT PARSER       │                                            |
|  │ Regex + entity extr │                                            |
|  │ → intent_t + params │                                            |
|  └──────────┬──────────┘                                            |
|             │                                                        |
|             v                                                        |
|  ┌─────────────────────┐                                            |
|  │ DIALOG MANAGER      │                                            |
|  │ State machine for   │                                            |
|  │ multi-turn dialogue │                                            |
|  └──────────┬──────────┘                                            |
|             │                                                        |
|             v                                                        |
|  ┌─────────────────────┐                                            |
|  │ TTS ENGINE           │                                            |
|  │ Concatenative synth  │                                            |
|  │ Pre-recorded phonemes│                                            |
|  │ Female voice, 48 kHz │                                            |
|  └──────────┬──────────┘                                            |
|             │                                                        |
|             v                                                        |
|  SPEAKER (Bone Conduction, I2S, MAX98357)                           |
+---------------------------------------------------------------------+
```

### 2.2 Voice Manager API

```c
// voice_manager.h

typedef enum {
    WAKE_STATE_IDLE,
    WAKE_STATE_LISTENING,
    WAKE_STATE_PROCESSING,
    WAKE_STATE_SLEEP
} wake_state_t;

typedef enum {
    DIALOG_STATE_IDLE,
    DIALOG_STATE_AWAITING_COMMAND,
    DIALOG_STATE_AWAITING_CONFIRMATION,
    DIALOG_STATE_AWAITING_DETAIL,
    DIALOG_STATE_PROCESSING,
    DIALOG_STATE_SPEAKING
} dialog_state_t;

typedef void (*voice_command_callback_t)(parsed_command_t* cmd);

int  voice_manager_init(void);
void voice_manager_start(void);
void voice_manager_stop(void);

// Wake word
void voice_set_wake_word(const char* phrase);
bool voice_is_wake_word_detected(void);
void voice_acknowledge_wake(void);  // Short beep

// Command
int  voice_process_command(const int16_t* audio, uint32_t len);
void voice_register_callback(voice_command_callback_t cb);

// TTS
int  voice_speak(const char* text);
int  voice_speak_urgent(const char* text);  // Interrupt current speech
int  voice_play_alert(alert_type_t type);
void voice_set_volume(uint8_t level);  // 0-10
void voice_stop_speaking(void);

// State
bool voice_is_currently_speaking(void);
bool voice_is_user_speaking(void);   // VAD status
wake_state_t voice_get_wake_state(void);
```

---

## 3. Speech Recognition

### 3.1 Pipeline

```
Raw Audio (I2S, 48 kHz, 16-bit)
    │
    v
Framing
    │ 32 ms frames, 10 ms hop
    │ 1536 samples per frame, 480 samples hop
    v
Feature Extraction (MFCC)
    │
    ├── Pre-emphasis: y[n] = x[n] - 0.97*x[n-1]
    ├── Hamming window
    ├── FFT (512-point, 48 kHz → 256 bins)
    ├── Mel filterbank (40 filters, 0-24 kHz)
    ├── Log compression: log(mel_energy)
    ├── DCT → 13 MFCC coefficients
    └── Delta + Delta-Delta → 39 features per frame
        │
        v
Feature Buffer
    │ Wake word model: 30 frames (300 ms) → 39×30
    │ Command model:   50 frames (500 ms) → 39×50
    v
Edge Impulse Inference
    │ DS-CNN model
    │ → class probabilities
    v
Decision
    │ Confidence > threshold → command accepted
    │ Confidence < threshold → ignored
```

### 3.2 MFCC Configuration

| Parameter | Value |
|---|---|
| Sample rate | 48 kHz |
| Frame size | 32 ms (1536 samples) |
| Hop size | 10 ms (480 samples) |
| Window | Hamming |
| FFT size | 512 |
| Mel filters | 40 |
| MFCC coefficients | 13 |
| Delta | Yes (13) |
| Delta-Delta | Yes (13) |
| **Total features** | **39 per frame** |
| Wake word frames | 30 (300 ms audio) |
| Command frames | 50 (500 ms audio) |

---

## 4. Wake Word Detection

### 4.1 Strategy

- Wake word: "Hey Glass"
- Always listening (even when system is "asleep" — low-power mode)
- Edge Impulse DS-CNN model (80 KB int8 quantized)
- Inference runs every 100 ms (10 Hz) on latest 300 ms audio window
- No action taken if confidence < 0.90

```c
// wake_word.c

#define WAKE_WORD_THRESHOLD      0.90f
#define WAKE_WORD_REJECT_THRESH  0.50f  // Below this = definitely not wake word
#define INFERENCE_INTERVAL_MS    100

static float mfcc_buffer[39][30];  // Ring buffer of MFCC frames
static uint8_t mfcc_index = 0;

void wake_word_add_frame(float mfcc[39]) {
    // Add new MFCC frame, shift buffer
    for (int i = 0; i < 29; i++) {
        memcpy(mfcc_buffer[i], mfcc_buffer[i+1], 39 * sizeof(float));
    }
    memcpy(mfcc_buffer[29], mfcc, 39 * sizeof(float));
}

int wake_word_detect(void) {
    // Run inference every INFERENCE_INTERVAL_MS
    static uint32_t last_inference_ms = 0;
    uint32_t now = xTaskGetTickCount();

    if (now - last_inference_ms < INFERENCE_INTERVAL_MS) {
        return 0;  // Not time yet
    }
    last_inference_ms = now;

    // Prepare signal for Edge Impulse
    ei_impulse_result_t result;
    int res = run_inference(mfcc_buffer, &wake_word_model, &result);

    if (res != 0) return 0;

    float confidence = result.classification[1].value;  // "wake word" class

    if (confidence >= WAKE_WORD_THRESHOLD) {
        logger_log(LOG_INFO, "Wake word detected (confidence: %.2f)", confidence);
        return 1;
    }

    return 0;
}
```

### 4.2 Wake Word Performance

| Metric | Target | Measured (estimated) |
|---|---|---|
| Detection rate (clean) | ≥ 99% | 99.5% |
| Detection rate (65 dB noise) | ≥ 95% | 96% |
| False accept rate | ≤ 0.1/hour | 0.05/hour |
| False reject rate | ≤ 1% | 0.5% |
| Inference time | ≤ 20 ms | 18 ms |
| Memory | ≤ 100 KB | 80 KB |

---

## 5. Noise Cancellation

### 5.1 Wiener Filter

```c
// noise_reduction.c

#define NOISE_FLOOR_UPDATE_MS  2000  // Re-estimate noise every 2s of silence

typedef struct {
    float noise_spectrum[256];       // Estimated noise power spectrum
    uint32_t last_update_ms;
    bool initialized;
} noise_estimator_t;

static noise_estimator_t noise;

void estimate_noise_floor(const float* fft_magnitude, int num_bins) {
    uint32_t now = xTaskGetTickCount();
    if (now - noise.last_update_ms < NOISE_FLOOR_UPDATE_MS) return;

    // Exponential moving average of noise spectrum
    for (int i = 0; i < num_bins; i++) {
        if (!noise.initialized) {
            noise.noise_spectrum[i] = fft_magnitude[i];
        } else {
            // Slow adaptation: 0.95 * old + 0.05 * new (during silence only)
            noise.noise_spectrum[i] = 0.95f * noise.noise_spectrum[i] +
                                       0.05f * fft_magnitude[i];
        }
    }

    noise.initialized = true;
    noise.last_update_ms = now;
}

void apply_wiener_filter(float* fft_magnitude, int num_bins) {
    if (!noise.initialized) return;

    for (int i = 0; i < num_bins; i++) {
        float signal_power = fft_magnitude[i] * fft_magnitude[i];
        float noise_power = noise.noise_spectrum[i] * noise.noise_spectrum[i];

        // Wiener gain: H = signal / (signal + noise)
        float gain = signal_power / (signal_power + noise_power + 1e-6f);

        // Apply gain floor (max attenuation: -20 dB)
        gain = fmax(gain, 0.1f);

        fft_magnitude[i] *= gain;
    }
}
```

### 5.2 Noise Reduction Performance

| Noise Condition | SNR Improvement | Expected Accuracy |
|---|---|---|
| Quiet (library) | 0 dB (no noise) | 99% |
| Street (65 dB) | +8 dB | 95% |
| Crowd (70 dB) | +10 dB | 90% |
| Wind (60 dB) | +5 dB | 88% |
| Kitchen (appliances) | +6 dB | 92% |

---

## 6. Intent Recognition

### 6.1 Intent Classes

```c
// intent_parser.h

typedef enum {
    // Navigation
    INTENT_NAVIGATE,       // "Navigate to [place]"
    INTENT_NAVIGATE_HOME,  // "Go home"
    INTENT_STOP,           // "Stop"
    INTENT_PAUSE,           // "Pause navigation"

    // Help & Info
    INTENT_HELP,           // "Help"
    INTENT_DESCRIBE,       // "What's around me?"
    INTENT_WHERE_AM_I,     // "Where am I?"
    INTENT_BATTERY,        // "Battery"
    INTENT_TIME,           // "What time is it?"

    // Reading
    INTENT_READ,           // "Read this"
    INTENT_READ_AGAIN,     // "Read again"
    INTENT_READ_SLOWER,    // "Read slower"

    // Reminders & Medicine
    INTENT_SET_REMINDER,   // "Set reminder [time] [description]"
    INTENT_MEDICINE,       // "My medicine"
    INTENT_SHOW_REMINDERS, // "What are my reminders?"

    // Safety
    INTENT_EMERGENCY,      // "Emergency"
    INTENT_CALL,           // "Call [contact]"
    INTENT_SOS,            // (fall detection automatic)

    // Settings
    INTENT_VOLUME_UP,      // "Volume up"
    INTENT_VOLUME_DOWN,    // "Volume down"
    INTENT_VOLUME_SET,     // "Volume [0-10]"
    INTENT_SILENT_MODE,    // "Silent mode"
    INTENT_PRIVACY_MODE,   // "Privacy mode"

    // System
    INTENT_CONNECT,        // "Connect to phone"
    INTENT_SLEEP,          // "Go to sleep"
    INTENT_RESTART,        // "Restart"

    INTENT_UNKNOWN         // Not recognized
} intent_t;
```

### 6.2 Entity Extraction

```c
// intent_parser.h

typedef struct {
    char key[16];
    char value[64];
} entity_t;

typedef struct {
    intent_t  intent;
    entity_t  entities[4];
    uint8_t   entity_count;
    float     confidence;
    char      raw_text[128];  // Raw command text for debugging
} parsed_command_t;

// Entity types extracted from command text:
// - destination: "home", "Central Park", "pharmacy"
// - time: "7:00 PM", "in 30 minutes", "tomorrow"
// - description: "take medicine", "call John"
// - contact: "John", "Mary", "Dr. Smith"
// - number: integer (volume level 0-10)
```

### 6.3 Intent Regex Patterns

```c
// intent_parser.c — Rule-based intent matching

parsed_command_t parse_intent(const char* command) {
    parsed_command_t result = {0};
    strncpy(result.raw_text, command, sizeof(result.raw_text) - 1);

    // Navigation
    if (match_pattern(command, "navigate to (.*)", result.entities[0].value)) {
        result.intent = INTENT_NAVIGATE;
        strcpy(result.entities[0].key, "destination");
        result.entity_count = 1;
    }
    else if (match_pattern(command, "go home", NULL)) {
        result.intent = INTENT_NAVIGATE_HOME;
    }
    else if (match_pattern(command, "stop", NULL)) {
        result.intent = INTENT_STOP;
    }

    // Help & Describe
    else if (match_pattern(command, "what('s| is) around me", NULL)) {
        result.intent = INTENT_DESCRIBE;
    }
    else if (match_pattern(command, "where am i", NULL)) {
        result.intent = INTENT_WHERE_AM_I;
    }
    else if (match_pattern(command, "help", NULL)) {
        result.intent = INTENT_HELP;
    }
    else if (match_pattern(command, "battery", NULL)) {
        result.intent = INTENT_BATTERY;
    }
    else if (match_pattern(command, "what time", NULL)) {
        result.intent = INTENT_TIME;
    }

    // Safety
    else if (match_pattern(command, "emergency", NULL)) {
        result.intent = INTENT_EMERGENCY;
    }
    else if (match_pattern(command, "call (.*)", result.entities[0].value)) {
        result.intent = INTENT_CALL;
        strcpy(result.entities[0].key, "contact");
        result.entity_count = 1;
    }

    // Settings
    else if (match_pattern(command, "volume up", NULL)) {
        result.intent = INTENT_VOLUME_UP;
    }
    else if (match_pattern(command, "volume down", NULL)) {
        result.intent = INTENT_VOLUME_DOWN;
    }
    else if (match_pattern(command, "volume ([0-9]+)", result.entities[0].value)) {
        result.intent = INTENT_VOLUME_SET;
        result.entity_count = 1;
    }
    else if (match_pattern(command, "silent mode", NULL)) {
        result.intent = INTENT_SILENT_MODE;
    }
    else if (match_pattern(command, "privacy mode", NULL)) {
        result.intent = INTENT_PRIVACY_MODE;
    }

    // Default
    else {
        result.intent = INTENT_UNKNOWN;
    }

    result.confidence = 1.0f;  // Deterministic matching
    return result;
}
```

---

## 7. Command Parsing

### 7.1 Command Flow

```
User: "Hey Glass"
  │
  ├── Wake word detected → Short acknowledgment beep
  │
  v
User: "Navigate to Central Park"
  │
  ├── 500 ms audio captured
  ├── Command model inference (30 ms)
  ├── Class: "navigate_to" (confidence: 0.93)
  ├── Intent parser: INTENT_NAVIGATE, destination = "Central Park"
  │
  v
Decision Engine: verify intent
  │
  ├── Is navigation already active?
  │   ├── Yes → "A route is already active. Cancel current route?"
  │   └── No → Process route request
  │
  v
"Navigating to Central Park. 1.2 kilometers."
  │
  ├── Spoken via TTS (bone conduction)
  └── Navigation started
```

### 7.2 Confidence Tiers

| Confidence | Action |
|---|---|
| ≥ 0.90 | Accept command, execute immediately |
| 0.75-0.89 | Accept command, confirm: "Did you say {interpretation}?" |
| 0.50-0.74 | Reject: "Sorry, I didn't understand. Please try again." |
| < 0.50 | Silently ignore (no response) |

---

## 8. Response Generation

### 8.1 Response Templates

```c
// response_generator.c

const char* get_response(intent_t intent, void* context) {
    switch (intent) {
        case INTENT_NAVIGATE: {
            char* dest = get_entity_value(context, "destination");
            float dist = get_nav_distance();
            static char buf[128];
            snprintf(buf, sizeof(buf),
                "Navigating to %s. %.1f kilometers.", dest, dist);
            return buf;
        }
        case INTENT_NAVIGATE_HOME: {
            return "Heading home.";
        }
        case INTENT_STOP:
            return "Navigation stopped.";
        case INTENT_DESCRIBE:
            return generate_scene_description();
        case INTENT_WHERE_AM_I:
            return generate_location_description();
        case INTENT_BATTERY:
            return generate_battery_status();
        case INTENT_HELP:
            return "Say 'navigate to' followed by a destination. "
                   "Say 'what's around me' for a description. "
                   "Say 'emergency' for help.";
        case INTENT_CALL: {
            char* contact = get_entity_value(context, "contact");
            static char buf[64];
            snprintf(buf, sizeof(buf), "Calling %s.", contact);
            return buf;
        }
        case INTENT_VOLUME_UP: {
            uint8_t vol = voice_get_volume() + 1;
            voice_set_volume(vol);
            static char buf[32];
            snprintf(buf, sizeof(buf), "Volume %d.", vol);
            return buf;
        }
        case INTENT_SILENT_MODE: {
            static bool silent = false;
            silent = !silent;
            return silent ?
                "Silent mode on. Haptic only." :
                "Silent mode off.";
        }
        case INTENT_UNKNOWN:
            return "Sorry, I didn't understand that command. "
                   "Say 'help' for available commands.";
        default:
            return "Command received.";
    }
}
```

---

## 9. Text-to-Speech

### 9.1 Concatenative TTS Architecture

```
Text Input
    │
    v
Text Normalizer
    │ "St. Mary's Hospital → "Saint Mary's Hospital"
    │ "500m" → "five hundred meters"
    │ "Dr." → "Doctor"
    v
Phrase Database (QSPI Flash)
    ├── Pre-recorded full phrases (critical alerts)
    │   "Emergency detected"
    │   "Fall detected"
    │   "Battery low"
    │   "Road ahead"
    │
    ├── Phoneme samples (1000+ individual phonemes)
    │   /ay/, /eh/, /s/, /t/, /n/, /ow/, /h/, /m/, ...
    │   Each: 16-bit, 48 kHz, ~50-200 ms duration
    │   Total: ~8 MB flash
    │
    └── Silence samples (50 ms, 100 ms, 500 ms)
    v
Phoneme Sequencer
    │ Convert text to phoneme sequence using dictionary
    │ "navigate" → /n/ /ae/ /v/ /ih/ /g/ /ey/ /t/
    │ Apply prosody: duration + pitch per phoneme
    v
Waveform Concatenator
    │ Stitch phoneme samples with cross-fade (5 ms)
    │ Apply volume envelope
    │ Apply equalization (bone conduction response)
    v
I2S DMA Buffer (ping-pong: 2 × 512 samples)
    │
    v
MAX98357 I2S Amplifier → Bone Conduction Speaker
```

### 9.2 TTS Performance

| Metric | Value |
|---|---|
| Voice quality | Natural (pre-recorded human voice) |
| Voice gender | Female (meditative, calming tone) |
| Speech rate | 160 wpm (configurable) |
| Response latency (direct) | < 50 ms (pre-recorded phrase) |
| Response latency (phoneme) | < 100 ms (assembled) |
| Storage for TTS | ~8 MB (QSPI flash) |
| Vocabulary | 2000+ words (phonetic dictionary) |

### 9.3 Urgency Tones

| Urgency | Tone | Volume | Example |
|---|---|---|---|
| Routine | Calm, normal pace | Current volume | "Continue straight for 100 meters" |
| Important | Slightly faster, firm | +2 over current | "Obstacle ahead, 2 meters. Stop." |
| Critical | Alert, commanding | Max volume | "Emergency! Fall detected!" |
| Emergency | Siren tone (1s) + voice | Max volume | [tone] "Emergency detected!" |

---

## 10. Conversation Flow

### 10.1 Dialog State Machine

```
                    ┌──────────┐
                    │  IDLE    │
                    │ (waiting │
                    │  for wake│
                    │  word)   │
                    └────┬─────┘
                         │ "Hey Glass" detected
                         v
                    ┌──────────┐
                    │ LISTENING│
                    │ (capture │
                    │  500 ms  │
                    │  audio)  │
                    └────┬─────┘
                         │ Audio captured
                         v
                    ┌──────────┐
                    │PROCESSING│
                    │ (model   │
                    │  infer)  │
                    └────┬─────┘
                         │
              ┌──────────┴──────────┐
              │                     │
              v                     v
        ┌──────────┐         ┌──────────┐
        │ AMBIGUOUS│         │ CONFIRMED│
        │ (75-89%  │         │ (≥ 90%  │
        │  conf)   │         │  conf)  │
        └────┬─────┘         └────┬─────┘
             │                    │
             v                    v
        ┌──────────┐         ┌──────────┐
        │ CONFIRM  │         │ EXECUTE  │
        │ "Did you │         │ Command  │
        │  say X?" │         │ + Respond│
        └────┬─────┘         └────┬─────┘
             │                    │
             │ User: "Yes"        │
             v                    v
        ┌──────────┐         ┌──────────┐
        │ EXECUTE  │         │ RETURN TO│
        │ Command  │         │ IDLE     │
        └────┬─────┘         └──────────┘
             │
             v
        ┌──────────┐
        │ RETURN TO│
        │ IDLE     │
        └──────────┘
```

### 10.2 Multi-turn Dialogue Examples

**Example 1: Navigation**

```
User:  "Hey Glass"               [Wake word → beep]
System: [Acknowledgment beep]     [Listening]
User:  "Navigate to hospital"     [Command]
System: "There are two hospitals nearby.
         Which one? Saint Mary's or City General?"
User:  "Saint Mary's"             [Response]
System: "Navigating to Saint Mary's Hospital.
         2.3 kilometers. Turn right on Main Street."
```

**Example 2: Reminder**

```
User:  "Hey Glass"
User:  "Set reminder"
System: "What time?"
User:  "7:00 PM"
System: "What should I remind you about?"
User:  "Take medicine"
System: "Reminder set for 7:00 PM: Take medicine."
```

**Example 3: Unknown command**

```
User:  "Hey Glass"
User:  "Play music"
System: "Sorry, I don't know how to do that.
         Available commands: navigate, describe, read,
         set reminder, emergency, help."
```

---

## 11. Emergency Commands

### 11.1 Emergency Flow

```
User: "Hey Glass, emergency"
     │
     v
CONFIRMATION PHASE
     │
     ├── Voice: "Emergency mode. Are you sure?"
     │
     ├── User: "Yes" or "No"
     │   ├── Yes → PROCEED
     │   └── No → "Emergency cancelled"
     │
     └── No response for 5 seconds → AUTO-PROCEED
          (in case user cannot speak)
          │
          v
ACTIVATION PHASE
     │
     ├── Voice: "Emergency activated. Stay calm. Help is on the way."
     ├── Haptic: SOS pattern (3 long, 3 short, 3 long) — repeat 3×
     ├── SMS to emergency contact:
     │     "EMERGENCY: Smart glasses user needs help.
     │      Location: {lat}, {lon}
     │      Time: {timestamp}
     │      Respond: 'OK' to confirm, 'HELP' for escalation"
     │
     ├── GPS tracking: 1 Hz position → log to flash
     ├── Microphone: listen for user or environmental sounds
     │
     v
MONITORING PHASE
     │
     ├── Check for SMS response from contact (via BLE phone relay)
     ├── Every 30 seconds: "Are you okay? Say 'I'm okay' to cancel."
     ├── After 5 minutes: escalate
     │     ├── Call emergency services (if phone connected)
     │     └── Send updated GPS coordinates
     │
     v
RESOLUTION
     ├── User: "I'm okay" → "Emergency cancelled. How do you feel?"
     ├── Contact SMS: "OK" → "Your contact has been notified. Help is coming."
     ├── Contact SMS: "HELP" → Escalate immediately
     └── 30 minute timeout → Auto-cancel, log event
```

---

## 12. Navigation Commands

| Command | Intent | Example |
|---|---|---|
| "Navigate to [place]" | NAVIGATE | "Navigate to Central Park" |
| "Go home" | NAVIGATE_HOME | "Go home" |
| "Stop" | STOP | "Stop" |
| "Pause" | PAUSE | "Pause navigation" |
| "Resume" | (auto: resume after pause) | — |
| "Where am I?" | WHERE_AM_I | "Where am I?" |
| "How much further?" | (auto: status check) | — |

### 12.1 Location Description Response

```c
char* generate_location_description(void) {
    static char buf[256];

    nav_position_t pos = nav_get_current_position();
    gps_reverse_geocode(&pos, buf, sizeof(buf));
    // buf: "123 Main Street, Springfield"

    scene_class_t scene = ai_get_last_scene();
    float heading = nav_get_heading();

    static char result[512];
    snprintf(result, sizeof(result),
        "You are at %s. You are %s. "
        "Facing %s. "
        "Nearby: %s. "
        "Battery is at %d%%.",
        buf,
        scene_to_string(scene),
        heading_to_cardinal(heading),
        generate_nearby_objects(),
        battery_get_level());

    return result;
}
```

---

## 13. Reading Commands

| Command | Intent | Description |
|---|---|---|
| "Read this" | READ | Trigger text recognition on camera frame |
| "Read again" | READ_AGAIN | Repeat last read text |
| "Read slower" | READ_SLOWER | Repeat last text at slower speed |

### 13.1 Reading Flow

```
User: "Hey Glass, read this"
     │
     v
Capture RGB frame (640×480)
     │
     v
Edge Impulse Text Recognition Model
     │ CRNN architecture (32×256 grayscale input)
     │ Output: character sequence via CTC decoding
     │
     ├── CER < 5%  → Read text normally
     ├── CER 5-10% → Read with caveat: "Text may be: {text}"
     └── CER > 10% → "Unable to read text clearly. Try better lighting."
     │
     v
System: "Text says: [content]"

User: "Read slower"
     │
     v
System: [Repeat text at 120 wpm instead of 160]

User: "Read again"
     │
     v
System: [Repeat text at current speed]
```

---

## 14. Reminder Commands

| Command | Intent | Example |
|---|---|---|
| "Set reminder" | SET_REMINDER | Multi-turn: set time + description |
| "My medicine" | MEDICINE | "Your next medicine is Aspirin at 7:00 PM" |
| "What reminders?" | SHOW_REMINDERS | List all pending reminders |

### 14.1 Reminder Scheduling

```c
// reminder_app.c

typedef struct {
    uint32_t id;
    uint8_t  hour;      // 0-23
    uint8_t  minute;    // 0-59
    uint8_t  day_of_week; // Bitmask: 0x01=Mon, 0x02=Tue, ...
    uint16_t day_of_month; // 0 = every month, 1-31 = specific day
    char     description[64];
    bool     active;
    bool     geo_triggered;  // True if location-based
    double   geo_lat;
    double   geo_lon;
    float    geo_radius_m;
} reminder_t;

// Reminder check runs every 30 seconds
void reminder_check(void) {
    uint32_t now = rtc_get_time();
    rtc_time_t t;
    rtc_get(&t);

    for (int i = 0; i < MAX_REMINDERS; i++) {
        if (!reminders[i].active) continue;

        bool time_match = (t.hour == reminders[i].hour &&
                           t.minute == reminders[i].minute);
        bool day_match = (reminders[i].day_of_week & (1 << (t.day_of_week - 1)));

        if (time_match && day_match) {
            // Trigger reminder
            char msg[128];
            snprintf(msg, sizeof(msg),
                "Reminder: %s", reminders[i].description);
            voice_speak(msg);

            // If medicine, include details
            if (strstr(reminders[i].description, "medicine") ||
                strstr(reminders[i].description, "medication")) {
                voice_speak("Please take your medication now.");
            }

            reminders[i].active = false;  // One-time
            db_save_reminders();
        }
    }
}
```

---

## 15. Accessibility Features

### 15.1 Alzheimer's-Specific Features

| Feature | Description | Implementation |
|---|---|---|
| Orientation reminder | Periodic time/location prompts | "You are at home. It is 7:00 PM." Every 30 min in home mode |
| Wandering alert | If user leaves geo-fence | "You are leaving your safe zone. Please return." |
| Repetition tolerance | Repeat prompts up to 3× | Decision engine: reprompt if no response |
| Calming voice | Always calm, never urgent (except real emergencies) | TTS voice selection + prosody |
| Simple vocabulary | Short, simple sentences | Response generator: max 10 words per prompt |
| Caregiver commands | "Call [caregiver]" | Speed-dial to pre-set contacts |
| Medicine reminders | Time-based medication alerts | Reminder system with repeat until confirmed |
| Confirmation prompts | Always confirm before actions | "Are you sure?" before non-critical actions |

### 15.2 Visually Impaired-Specific Features

| Feature | Description |
|---|---|
| Zero visual dependency | No touch screen required for operation |
| Haptic feedback | Silent mode alerts, command confirmation |
| Audio speed control | Configurable speech rate (100-200 wpm) |
| Volume boost | Up to 80 dB for hearing-impaired users |
| Audio description | Automatic scene and object description |
| Text reading | On-demand text recognition |
| Obstacle sonification | Future: distance-to-audio mapping |

---

## 16. Dialogue Flow Diagrams

### 16.1 Complete Dialogue Flow

```
                    ┌─────────────────────────┐
                    │       IDLE              │
                    │  Wake word listening    │
                    │  (low power)            │
                    └───────────┬─────────────┘
                                │ "Hey Glass"
                                v
                    ┌─────────────────────────┐
                    │    WAKE CONFIRMED        │
                    │  [Short acknowledgment   │
                    │   beep — 100 ms, 1 kHz]  │
                    └───────────┬─────────────┘
                                │
                                v
                    ┌─────────────────────────┐
                    │    LISTENING            │
                    │  Capture 500 ms audio   │
                    │  LED: breathing blue    │
                    └───────────┬─────────────┘
                                │
                    ┌───────────┴─────────────┐
                    │                         │
                    v                         v
          ┌─────────────────┐       ┌─────────────────┐
          │ COMMAND         │       │ NO COMMAND      │
          │ RECOGNIZED      │       │ (silence/timeout)│
          │ (conf ≥ 0.90)   │       └────────┬────────┘
          └────────┬────────┘                │
                   │                         v
                   v                ┌─────────────────┐
          ┌─────────────────┐       │  RETURN TO IDLE  │
          │ INTENT PARSED   │       │  (no response)   │
          └────────┬────────┘       └──────────────────┘
                   │
          ┌────────┴────────┐
          │                 │
          v                 v
   ┌──────────────┐   ┌──────────────┐
   │ AMBIGUOUS    │   │ CONFIRMED    │
   │ (75-89%)     │   │ (≥ 90%)      │
   └──────┬───────┘   └──────┬───────┘
          │                  │
          v                  v
   ┌──────────────┐   ┌──────────────┐
   │ "Did you     │   │ EXECUTE      │
   │  say X?"     │   │ Command      │
   └──────┬───────┘   └──────┬───────┘
          │                  │
     ┌────┴────┐             │
     │         │             │
     v         v             v
  ┌──────┐ ┌──────┐   ┌──────────────┐
  │"Yes" │ │"No"  │   │ RESPOND      │
  └──┬───┘ └──┬───┘   └──────┬───────┘
     │        │              │
     v        v              │
  ┌──────┐ ┌──────┐         │
  │EXEC  │ │RETRY │         │
  └──┬───┘ └──┬───┘         │
     │        │              │
     │  ┌─────┴─────┐       │
     │  │ LISTEN    │       │
     │  │ AGAIN     │       │
     │  └───────────┘       │
     │                      │
     └──────────┬───────────┘
                │
                v
     ┌──────────────────────┐
     │   MULTI-TURN CHECK   │
     │  Does command need   │
     │  more information?   │
     └──────┬───────┬───────┘
            │       │
            v       v
       ┌────────┐ ┌──────────────┐
       │ YES    │ │ NO           │
       │ (ask   │ │ (return to   │
       │ detail)│ │  IDLE)       │
       └───┬────┘ └──────────────┘
           │
           v
       ┌────────────┐
       │ LISTEN     │
       │ FOR DETAIL │
       └────────────┘
```

---

## 17. Sequence Diagrams

### 17.1 Voice Command Processing Sequence

```
User      Microphone     Audio Preproc    Edge Impulse    Intent Parser    Dialog Mgr      TTS        Speaker
 │            │               │               │               │               │           │           │
 │"Hey Glass" │               │               │               │               │           │           │
 ├───────────▶│               │               │               │               │           │           │
 │            │ 1536 samples  │               │               │               │           │           │
 │            ├──────────────▶│               │               │               │           │           │
 │            │               │ MFCC (39×30) │               │               │           │           │
 │            │               ├──────────────▶               │               │           │           │
 │            │               │               │ Inference    │               │           │           │
 │            │               │               ├── DS-CNN ───▶│               │           │           │
 │            │               │               │ conf: 0.95   │               │           │           │
 │            │               │               │              │               │           │           │
 │            │               │               │ Wake word    │               │           │           │
 │            │               │               │ detected     │               │           │           │
 │            │               │               ├──────────────▶│               │           │           │
 │            │               │               │              │ State:        │           │           │
 │            │               │               │              │ LISTENING     │           │           │
 │            │               │               │              ├──────────────▶│           │           │
 │            │               │               │              │               │ Beep cmd │           │
 │            │               │               │              │               ├───────────▶          │
 │            │               │               │              │               │           │[beep]    │
 │ ◀═══════════════════════════════════════════════════════════════════════════════════════║════════│
 │            │               │               │              │               │           │          │
 │"Navigate   │               │               │              │               │           │          │
 │ to park"   │               │               │              │               │           │          │
 ├───────────▶│               │               │              │               │           │          │
 │            │ 500ms audio   │               │              │               │           │          │
 │            ├──────────────▶│               │              │               │           │          │
 │            │               │ MFCC (39×50) │              │               │           │          │
 │            │               ├──────────────▶               │               │           │          │
 │            │               │               │ Inference    │               │           │          │
 │            │               │               ├── DS-CNN ───▶│               │           │          │
 │            │               │               │ class: nav   │               │           │          │
 │            │               │               │ conf: 0.93   │               │           │          │
 │            │               │               │              │               │           │          │
 │            │               │               │              │ Parse:        │           │          │
 │            │               │               │              │ INTENT_NAV    │           │          │
 │            │               │               │              │ dest: park    │           │          │
 │            │               │               │              ├──────────────▶│           │          │
 │            │               │               │              │               │ Generate  │          │
 │            │               │               │              │               │ response  │          │
 │            │               │               │              │               ├──────────▶│          │
 │            │               │               │              │               │           │ "Navigat │
 │            │               │               │              │               │           │  ing to  │
 │            │               │               │              │               │           │  park..."│
 │ ◀═══════════════════════════════════════════════════════════════════════════════════════║════════│
```

---

## 18. Architecture Diagrams

### 18.1 Voice Software Module Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    VOICE SOFTWARE MODULES                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    VOICE MANAGER                             ││
│  │  (Orchestrates all voice subsystems)                        ││
│  └──────────────────────┬──────────────────────────────────────┘│
│                         │                                        │
│         ┌───────────────┼───────────────┐                        │
│         │               │               │                        │
│         v               v               v                        │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                 │
│  │ AUDIO IN   │  │ AUDIO OUT  │  │ DIALOG     │                 │
│  │ MODULE     │  │ MODULE     │  │ MANAGER    │                 │
│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘                 │
│         │               │               │                        │
│  ┌──────┴─────┐  ┌──────┴─────┐  ┌──────┴─────┐                 │
│  │ VAD        │  │ TTS        │  │ INTENT     │                 │
│  │ Detector   │  │ Engine     │  │ Parser     │                 │
│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘                 │
│         │               │               │                        │
│  ┌──────┴─────┐  ┌──────┴─────┐  ┌──────┴─────┐                 │
│  │ Noise      │  │ Prosody    │  │ Entity     │                 │
│  │ Reduction  │  │ Generator  │  │ Extractor  │                 │
│  └──────┬─────┘  └────────────┘  └──────┬─────┘                 │
│         │                               │                        │
│  ┌──────┴─────┐                  ┌──────┴─────┐                 │
│  │ Wake Word  │                  │ Response   │                 │
│  │ Detector   │                  │ Generator  │                 │
│  │ (EI model) │                  └────────────┘                 │
│  └──────┬─────┘                                                 │
│         │                                                        │
│  ┌──────┴─────┐                                                 │
│  │ Command    │                                                 │
│  │ Recognizer │                                                 │
│  │ (EI model) │                                                 │
│  └────────────┘                                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Audio/AI Engineer | Initial draft |

---

*End of Document — VOICE-ARCH-001*
