# Stereo Imager

A stereo width/imaging audio plugin built with JUCE, featuring a real-time vectorscope visualizer, phase correlation meter, and a suite of stereo-shaping tools (width, balance, mid/side, crossfeed, and exciter/enhancer).

## Features

- **Width control** — adjustable stereo width via mid/side processing
- **Balance** — left/right channel balance
- **Input / Output Gain** — pre and post gain staging (-60 dB to +24 dB)
- **Mid/Side processing** — independent mid/side blend control
- **Crossfeed** — blends left and right channels for mono-compatibility checks
- **Exciter/Enhancer** — simple harmonic excitation for added presence
- **Vectorscope** — real-time stereo field visualizer with persistent density trails
- **Phase Correlation Meter** — live correlation readout with bar meter, numeric display, and history sparkline

## Screenshots

*(Add screenshots of the plugin UI here)*

## Requirements

- **JUCE** (developed with Projucer — see `.jucer` file for module configuration)
- **Xcode** (macOS build target)
- macOS 10.13+ (adjust based on your deployment target in Projucer)

## Building

This project is developed using **Projucer** and **Xcode**.

1. Open `StereoImager.jucer` in Projucer (or your project's `.jucer` file)
2. Ensure your JUCE modules path is correctly set in Projucer's global paths
3. Click **Save and Open in IDE** to regenerate the Xcode project and open it
4. Build in Xcode (`Cmd + B`) — this compiles both the **VST3** and **AU** (Audio Unit) plugin formats, depending on your Projucer export settings

> **Note:** `JuceLibraryCode/` and the `Builds/` output directories are regenerated automatically by Projucer/Xcode and are not tracked in version control (see `.gitignore`).

## Project Structure

```
StereoImager/
├── StereoImager.jucer          # Projucer project file
├── Source/
│   ├── PluginProcessor.cpp/h   # Audio processing & parameter management
│   ├── PluginEditor.cpp/h      # Main plugin UI
│   ├── Param.h                 # Parameter ID definitions
│   ├── CustomLookAndFeel.cpp/h              # Rotary knob & popup/combo styling
│   ├── CustomLookAndFeelVerticalSlider.cpp/h # Vertical fader styling
│   ├── VisualizerComponent.cpp/h            # Stereo vectorscope
│   ├── PhaseCorrelationMeter.cpp/h          # Phase correlation display
│   └── PluginFonts.h            # Shared typeface loading (Fjalla One)
├── Builds/                     # (generated, ignored)
└── JuceLibraryCode/             # (generated, ignored)
```

## Parameters

| Parameter | Range | Default | Description |
|---|---|---|---|
| Width | 0–100% | 50% | Stereo width via mid/side blend |
| Balance | -1.0 – 1.0 | 0.0 | Left/right balance |
| Input Gain | -60 – 24 dB | 0 dB | Pre-processing gain |
| Output Gain | -60 – 24 dB | 0 dB | Post-processing gain |
| Mid/Side | -1.0 – 1.0 | 0.0 | Mid/side blend amount |
| Stereo Spread | 0–100% | 50% | *(reserved / in development)* |
| Crossfeed | 0.0 – 1.0 | 0.0 | Channel crossfeed amount |
| Exciter/Enhancer | 0–100% | 0% | Harmonic excitation amount |

## Signal Flow

1. Input gain applied
2. Dry/wet mixing prepared (width control)
3. Width (mid/side) processing
4. Balance processing
5. Mid/side blend
6. Crossfeed
7. Output gain
8. Exciter/enhancer (harmonic excitation)
9. Audio data captured (thread-safe FIFO) for visualizer and phase meter

## Visualizer Notes

The vectorscope (`VisualizerComponent`) uses a persistent density-grid accumulation technique with a fading dot-spray layer, rendered entirely via CPU software rasterization for consistent cross-platform appearance. It's capped at 30fps and automatically stops repainting during periods of silence to conserve CPU.

The phase correlation meter (`PhaseCorrelationMeter`) computes real-time correlation between left/right channels and displays it via a bar meter, large numeric readout, and a rolling sparkline history.
