/*
  ==============================================================================
    CustomLookAndFeelVerticalSlider.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginFonts.h"

class CustomLookAndFeelVerticalSlider : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeelVerticalSlider();
    ~CustomLookAndFeelVerticalSlider() override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override;

    juce::Font getLabelFont(juce::Label& label) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override
    {
        return PluginFonts::getTypeface();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeelVerticalSlider)
};
