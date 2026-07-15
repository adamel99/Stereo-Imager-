/*
  ==============================================================================
    VisualizerComponent.h  –  Stereo Imager vectorscope

    Visual style: dot/particle accumulation with persistent density grid.
    - Colour ramp: dark magenta (sparse) → pink → white-hot (dense)
    - Blue sphere body with dashed grid lines and L/R/C labels
    - All rendering is CPU software-rasterised (no OpenGL)

    Performance notes:
    - densityGridSize kept at 64 (16x fewer cells than 256)
    - fadeImagePixels replaced with a single translucent fillAll pass
    - Timer capped at 30fps even when active
    - repaint() skipped entirely when signal has been silent for a while
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

class VisualizerComponent : public juce::Component, private juce::Timer
{
public:
    VisualizerComponent();
    ~VisualizerComponent() override;

    void setAudioData(const std::vector<float>& leftChannelData,
                      const std::vector<float>& rightChannelData);
    void copyAudioBuffer(const juce::AudioBuffer<float>& buffer);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSphere(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawLabels(juce::Graphics& g);
    void buildAndRenderFrame();

    float        getVizRadius() const;
    void         ensureImages();
    juce::Colour densityToColour(float density) const;

    juce::CriticalSection lock;
    std::vector<float> leftChannelData;
    std::vector<float> rightChannelData;

    // Compositing layers — all CPU-rasterised for cross-platform consistency
    juce::Image compositeImage;   // final result, blitted in paint()
    juce::Image sphereCache;      // static sphere + grid, rebuilt only on resize
    juce::Image dotLayer;         // fading dot accumulation layer

    bool gridNeedsRedraw = true;

    bool  hasRecentActivity = false;
    int   inactivityCounter = 0;
    float peakMagnitude     = 0.0f;

    // Persistent density histogram — accumulates dot hits, decays per frame.
    // Reduced to 64x64 for CPU performance (visually equivalent to 256x256).
    static constexpr int densityGridSize = 64;
    std::array<std::array<float, densityGridSize>, densityGridSize> densityGrid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerComponent)
};
