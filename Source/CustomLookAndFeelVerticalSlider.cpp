/*
  ==============================================================================
    CustomLookAndFeelVerticalSlider.cpp
  ==============================================================================
*/

#include "CustomLookAndFeelVerticalSlider.h"

CustomLookAndFeelVerticalSlider::CustomLookAndFeelVerticalSlider() {}
CustomLookAndFeelVerticalSlider::~CustomLookAndFeelVerticalSlider() {}

//==============================================================================
void CustomLookAndFeelVerticalSlider::drawLinearSlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // ── Palette ───────────────────────────────────────────────────────────
    const juce::Colour textPrimary  { 0xFFE2E2EC };
    const juce::Colour borderMid    { 0xFF2A2A38 };
    const juce::Colour discFill     { 0xFF1E1E2A };
    const juce::Colour pink         { 0xFFF86F6F };
    const juce::Colour blue         { 0xFF4646C8 };

    const juce::String name = slider.getName();
    const bool isPink = (name == "drive" || name == "ceiling");
    const juce::Colour arcCol = isPink ? pink : blue;

    // ── Geometry ──────────────────────────────────────────────────────────
    const float trackW      = 15.0f;
    const float thumbR      = 10.0f;
    const float cx          = (float)x + (float)width * 0.5f;
    const float trackTop    = (float)y + thumbR;
    const float trackBottom = (float)y + (float)height - thumbR;

    // Clamp thumb so it never leaves the track
    const float thumbY = juce::jlimit(trackTop, trackBottom, sliderPos);

    // ── 1. Track ──────────────────────────────────────────────────────────
    {
        juce::Path track;
        track.addRoundedRectangle(cx - trackW * 0.5f, trackTop,
                                  trackW, trackBottom - trackTop, trackW * 0.5f);
        g.setColour(borderMid);
        g.fillPath(track);
    }

    // ── 2. Value fill — clipped to track pill ─────────────────────────────
    if (thumbY < trackBottom)
    {
        juce::Path trackClip;
        trackClip.addRoundedRectangle(cx - trackW * 0.5f, trackTop,
                                      trackW, trackBottom - trackTop, trackW * 0.5f);
        g.saveState();
        g.reduceClipRegion(trackClip);

        juce::Path fill;
        fill.addRoundedRectangle(cx - trackW * 0.5f, thumbY,
                                 trackW, trackBottom - thumbY, trackW * 0.5f);
        g.setColour(arcCol);
        g.fillPath(fill);

        g.restoreState();
    }

    // ── 3. Thumb disc ─────────────────────────────────────────────────────
    g.setColour(discFill);
    g.fillEllipse(cx - thumbR, thumbY - thumbR, thumbR * 2.f, thumbR * 2.f);

    g.setColour(borderMid);
    g.drawEllipse(cx - thumbR, thumbY - thumbR, thumbR * 2.f, thumbR * 2.f, 1.0f);

    // Tick line through thumb centre
    const float tickHalf = thumbR * 0.5f;
    g.setColour(textPrimary);
    g.drawLine(cx - tickHalf, thumbY, cx + tickHalf, thumbY, 1.5f);
}

//==============================================================================
juce::Font CustomLookAndFeelVerticalSlider::getLabelFont(juce::Label& label)
{
    // Upsize the slider value text box; leave other labels at their set size
    if (label.getName().contains("textBox") || label.getName().isEmpty())
        return PluginFonts::get(20.0f);

    return PluginFonts::get(label.getFont().getHeight());
}

//==============================================================================
void CustomLookAndFeelVerticalSlider::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(juce::Colours::transparentBlack);

    if (!label.isBeingEdited())
    {
        g.setColour(juce::Colour(0xFFE2E2EC));
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(),
                         label.getLocalBounds(),
                         label.getJustificationType(),
                         1,
                         1.0f);
    }
}

//==============================================================================
juce::Slider::SliderLayout CustomLookAndFeelVerticalSlider::getSliderLayout(juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;

    const auto bounds  = slider.getLocalBounds();
    const int textBoxH = 30;    // ↑ was 26 — match the value in PluginEditor.cpp
    const int textBoxW = 200;

    layout.textBoxBounds = juce::Rectangle<int>(
        (bounds.getWidth() - textBoxW) / 2,
        bounds.getHeight() - textBoxH,
        textBoxW,
        textBoxH
    );

    layout.sliderBounds = juce::Rectangle<int>(
        0,
        0,
        bounds.getWidth(),
        bounds.getHeight() - textBoxH - 2
    );

    return layout;
}
