/*
  ==============================================================================
    PhaseCorrelationMeter.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PhaseCorrelationMeter : public juce::Component, private juce::Timer
{
public:
    PhaseCorrelationMeter();
    ~PhaseCorrelationMeter() override;

    void setAudioData(const std::vector<float>& leftChannelData,
                      const std::vector<float>& rightChannelData);
    void paint   (juce::Graphics& g) override;
    void resized ()                  override;

private:
    void  timerCallback() override;
    float calculatePhaseCorrelation();

    void drawBarMeter (juce::Graphics& g, juce::Rectangle<float> area);
    void drawReadout  (juce::Graphics& g, juce::Rectangle<float> area);
    void drawSparkline(juce::Graphics& g, juce::Rectangle<float> area);

    juce::CriticalSection lock;
    std::vector<float>    leftChannelData;
    std::vector<float>    rightChannelData;
    float                 phaseCorrelation = 0.0f;
    std::vector<float>    phaseHistory;
    bool                  isActive         = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseCorrelationMeter)
};
