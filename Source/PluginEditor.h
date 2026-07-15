/*
  ==============================================================================
    PluginEditor.h  –  Stereo Imager  (cross-platform update)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VisualizerComponent.h"
#include <optional>
#include "CustomLookAndFeel.h"
#include "PhaseCorrelationMeter.h"
#include "CustomLookAndFeelVerticalSlider.h"

class PracticeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    PracticeAudioProcessorEditor(PracticeAudioProcessor&);
    ~PracticeAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;

    void setScaleFactor(float newScale) override
    {
        currentScaleFactor = newScale;
        setTransform(juce::AffineTransform::scale(newScale));
    }

    VisualizerComponent   visualizer;
    PhaseCorrelationMeter phaseCorrelationMeter;

private:
    void timerCallback() override;

    void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> area,
                             bool accent = false);
    void drawSectionLabel   (juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<float> area);
    // NEW: column index badge (e.g. "01", "02")
    void drawColumnBadge    (juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<float> area);

    PracticeAudioProcessor& audioProcessor;

    // ── Sliders ───────────────────────────────────────────────────────────────
    juce::Slider widthSlider;
    juce::Slider balanceSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::Slider midSideSlider;
    juce::Slider crossfeedSlider;
    juce::Slider exciterEnhancerSlider;

    // ── Labels ────────────────────────────────────────────────────────────────
    juce::Label widthLabel;
    juce::Label balanceLabel;
    juce::Label inputGainLabel;
    juce::Label outputGainLabel;
    juce::Label midSideLabel;
    juce::Label crossfeedLabel;
    juce::Label exciterEnhancerLabel;
    juce::Label pluginNameLabel;

    // ── Attachments ───────────────────────────────────────────────────────────
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> balanceAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> midSideAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> crossfeedAttachment;
    std::optional<juce::AudioProcessorValueTreeState::SliderAttachment> exciterEnhancerAttachment;

    // ── Look & Feel ───────────────────────────────────────────────────────────
    CustomLookAndFeel               customLookAndFeel;
    CustomLookAndFeelVerticalSlider customLookAndFeelVerticalSlider;

    float currentScaleFactor = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PracticeAudioProcessorEditor)
};
