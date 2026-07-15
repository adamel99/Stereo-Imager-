/*
  ==============================================================================
    PluginEditor.cpp  –  Stereo Imager  (cross-platform update)
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Param.h"
#include "CustomLookAndFeel.h"
#include "CustomLookAndFeelVerticalSlider.h"

// ── Colour palette ─────────────────────────────────────────────────────────
static const juce::Colour kBg      { 0xFF0E0E12 };
static const juce::Colour kPanel   { 0xFF13131A };
static const juce::Colour kBorder  { 0xFF2A2A38 };
static const juce::Colour kAccent  { 0xFF4646C8 };
static const juce::Colour kAccent2 { 0xFFF86F6F };
static const juce::Colour kText    { 0xFFE2E2EC };
static const juce::Colour kMuted   { 0xFFAAAAAC };
static const juce::Colour kTrack   { 0xFF1E1E2A };

// ── Fixed logical pixel dimensions ─────────────────────────────────────────
static constexpr int kW       = 960;
static constexpr int kH       = 540;
static constexpr int kHeaderH = 44;
static constexpr int kBottomH = 130;   // ← was 190; smaller bottom = taller top
static constexpr int kPhaseW  = 280;
static constexpr int kIOW     = 160;
static constexpr int kFaderW  = 110;
static constexpr int kPad     = 8;

// ── Derived font sizes ──────────────────────────────────────────────────────
static constexpr float kFontSection = kH * 0.032f;   // was 0.026f
static constexpr float kFontHeader  = kH * 0.036f;   // was 0.0296f
static constexpr float kFontLabel   = kH * 0.036f;   // was 0.030f
static constexpr float kFontBadge   = 9.0f;          // was 8.0f

// ── Layout helper ───────────────────────────────────────────────────────────
struct Layout
{
    int top, topH, botY;
    int vizX, vizW;
    int ioX;
    int faderX, faderPanelW;
    int knobPanelX, knobPanelW;

    static Layout compute()
    {
        Layout L;
        L.top         = kHeaderH + kPad;
        L.topH        = kH - kHeaderH - kBottomH - kPad * 3;
        L.botY        = kH - kBottomH - kPad;

        L.vizX        = kPad;
        L.vizW        = kW - kIOW - kFaderW - kPad * 4;

        L.ioX         = L.vizX + L.vizW + kPad;
        L.faderX      = L.ioX  + kIOW   + kPad;
        L.faderPanelW = kFaderW - kPad;

        L.knobPanelX  = kPad + kPhaseW + kPad;
        L.knobPanelW  = kW - kPhaseW - kPad * 3;
        return L;
    }
};

//==============================================================================
PracticeAudioProcessorEditor::PracticeAudioProcessorEditor(PracticeAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kW, kH);
    setResizable(false, false);
    setOpaque(true);

    // ── Look & Feel ──────────────────────────────────────────────────────────
    widthSlider          .setLookAndFeel(&customLookAndFeelVerticalSlider);
    inputGainSlider      .setLookAndFeel(&customLookAndFeelVerticalSlider);
    outputGainSlider     .setLookAndFeel(&customLookAndFeelVerticalSlider);
    balanceSlider        .setLookAndFeel(&customLookAndFeel);
    midSideSlider        .setLookAndFeel(&customLookAndFeel);
    crossfeedSlider      .setLookAndFeel(&customLookAndFeel);
    exciterEnhancerSlider.setLookAndFeel(&customLookAndFeel);

    // ── Width (vertical fader) ───────────────────────────────────────────────
    addAndMakeVisible(widthSlider);
    widthSlider.setSliderStyle(juce::Slider::LinearVertical);
    widthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
    widthSlider.setRange(0.0, 100.0, 0.01);
    widthSlider.setValue(*audioProcessor.getWidthParam());

    // ── Vertical gain faders ─────────────────────────────────────────────────
    auto setupVertical = [&](juce::Slider& s, double lo, double hi, double val,
                             const juce::String& name)
    {
        addAndMakeVisible(s);
        s.setName(name);
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
        s.setRange(lo, hi, 0.01);
        s.setValue(val);
    };

    setupVertical(inputGainSlider,  -60.0, 24.0, *audioProcessor.getInputGainParam(),  "inputGain");
    setupVertical(outputGainSlider, -60.0, 24.0, *audioProcessor.getOutputGainParam(), "outputGain");

    inputGainSlider.textFromValueFunction  = [](double v)
        { return juce::String(v, 1) + " dB"; };
    outputGainSlider.textFromValueFunction = [](double v)
        { return juce::String(v, 1) + " dB"; };

    // ── Rotary knobs ─────────────────────────────────────────────────────────
    auto setupRotary = [&](juce::Slider& s, double lo, double hi, double val,
                           const juce::String& name)
    {
        addAndMakeVisible(s);
        s.setName(name);
        s.setSliderStyle(juce::Slider::Rotary);
        // ↓ text box wider and taller so values aren't clipped
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);  // was 80, 18
        s.setRange(lo, hi, 0.01);
        s.setValue(val);
    };

    setupRotary(balanceSlider,          -1.0,  1.0, *audioProcessor.getBalanceParam(),         "balance");
    setupRotary(midSideSlider,         -60.0, 24.0, *audioProcessor.getOutputGainParam(),      "midSide");
    setupRotary(crossfeedSlider,         0.0,  1.0, *audioProcessor.getCrossfeedParam(),       "crossfeed");
    setupRotary(exciterEnhancerSlider,   0.0,  1.0, *audioProcessor.getExciterEnhancerParam(), "exciterEnhancer");

    midSideSlider.textFromValueFunction = [](double v)
        { return juce::String(v, 1) + " dB"; };

    // ── Labels ────────────────────────────────────────────────────────────────
    auto setupLabel = [&](juce::Label& lbl, const juce::String& text,
                           juce::Component& target)
     {
         addAndMakeVisible(lbl);
         lbl.setText(text, juce::dontSendNotification);
         lbl.setJustificationType(juce::Justification::centred);
         // ↓ was kMuted — now kText so label names are clearly visible
         lbl.setColour(juce::Label::textColourId, kText);
         lbl.setFont(PluginFonts::get(kFontLabel));
         lbl.attachToComponent(&target, false);
     };

    setupLabel(inputGainLabel,       "INPUT",     inputGainSlider);
    setupLabel(outputGainLabel,      "OUTPUT",    outputGainSlider);
    setupLabel(balanceLabel,         "BALANCE",   balanceSlider);
    setupLabel(midSideLabel,         "MID/SIDE",  midSideSlider);
    setupLabel(crossfeedLabel,       "CROSSFEED", crossfeedSlider);
    setupLabel(exciterEnhancerLabel, "EXCITER",   exciterEnhancerSlider);

    // Width label — NOT attached; manually placed in resized()
    addAndMakeVisible(widthLabel);
    widthLabel.setText("WIDTH", juce::dontSendNotification);
    widthLabel.setJustificationType(juce::Justification::centred);
    widthLabel.setColour(juce::Label::textColourId, kText); 
    widthLabel.setFont(PluginFonts::get(kFontLabel));

    // ── Header label ──────────────────────────────────────────────────────────
    addAndMakeVisible(pluginNameLabel);
    pluginNameLabel.setText("STEREO IMAGER", juce::dontSendNotification);
    pluginNameLabel.setFont(PluginFonts::get(kFontHeader));
    pluginNameLabel.setColour(juce::Label::textColourId, kText);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);

    // ── Visualizers ───────────────────────────────────────────────────────────
    addAndMakeVisible(visualizer);
    addAndMakeVisible(phaseCorrelationMeter);

    // ── Attachments ───────────────────────────────────────────────────────────
    widthAttachment          .emplace(audioProcessor.apvts, ParamIDs::width,      widthSlider);
    balanceAttachment        .emplace(audioProcessor.apvts, ParamIDs::balance,    balanceSlider);
    inputGainAttachment      .emplace(audioProcessor.apvts, ParamIDs::inputGain,  inputGainSlider);
    outputGainAttachment     .emplace(audioProcessor.apvts, ParamIDs::outputGain, outputGainSlider);
    midSideAttachment        .emplace(audioProcessor.apvts, ParamIDs::midSide,    midSideSlider);
    crossfeedAttachment      .emplace(audioProcessor.apvts, "crossfeed",          crossfeedSlider);
    exciterEnhancerAttachment.emplace(audioProcessor.apvts, "exciterEnhancer",    exciterEnhancerSlider);

    startTimerHz(60);
}

PracticeAudioProcessorEditor::~PracticeAudioProcessorEditor()
{
    stopTimer();
    widthSlider          .setLookAndFeel(nullptr);
    inputGainSlider      .setLookAndFeel(nullptr);
    outputGainSlider     .setLookAndFeel(nullptr);
    balanceSlider        .setLookAndFeel(nullptr);
    midSideSlider        .setLookAndFeel(nullptr);
    crossfeedSlider      .setLookAndFeel(nullptr);
    exciterEnhancerSlider.setLookAndFeel(nullptr);
}

//==============================================================================
void PracticeAudioProcessorEditor::drawPanelBackground(juce::Graphics& g,
                                                        juce::Rectangle<float> area,
                                                        bool accent)
{
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(area.translated(0, 3).expanded(1), 8.0f);

    juce::ColourGradient grad(kPanel.brighter(0.04f), area.getX(), area.getY(),
                               kPanel.darker(0.12f),   area.getX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(area, 8.0f);

    g.setColour(kBorder);
    g.drawRoundedRectangle(area, 8.0f, 1.0f);

    if (accent)
    {
        juce::ColourGradient glow(kAccent.withAlpha(0.55f), area.getX(),    area.getY(),
                                   kAccent.withAlpha(0.0f),  area.getRight(), area.getY(), false);
        g.setGradientFill(glow);
        juce::Path topEdge;
        topEdge.startNewSubPath(area.getX() + 8.0f, area.getY());
        topEdge.lineTo(area.getRight() - 8.0f,       area.getY());
        g.strokePath(topEdge, juce::PathStrokeType(1.5f));
    }
}

void PracticeAudioProcessorEditor::drawSectionLabel(juce::Graphics& g,
                                                     const juce::String& text,
                                                     juce::Rectangle<float> area)
{
    g.setFont(PluginFonts::get(kFontSection));
    g.setColour(kMuted.brighter(0.3f));
    g.drawText(text, area, juce::Justification::centredLeft);
}

void PracticeAudioProcessorEditor::drawColumnBadge(juce::Graphics& g,
                                                    const juce::String& text,
                                                    juce::Rectangle<float> area)
{
    g.setFont(PluginFonts::get(kFontBadge));
    g.setColour(kBorder.brighter(0.3f));
    g.drawText(text, area, juce::Justification::centred);
}

//==============================================================================
void PracticeAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto lay = Layout::compute();

    g.fillAll(kBg);

    // Ambient glow
    {
        juce::ColourGradient ambient(kAccent.withAlpha(0.04f),
                                     kW * 0.38f, kH * 0.38f,
                                     juce::Colours::transparentBlack,
                                     kW * 0.38f, kH * 1.0f, true);
        g.setGradientFill(ambient);
        g.fillRect(getLocalBounds());
    }

    // ── Header ────────────────────────────────────────────────────────────────
    {
        juce::ColourGradient hg(kPanel.brighter(0.06f), 0, 0,
                                 kPanel,                  0, (float)kHeaderH, false);
        g.setGradientFill(hg);
        g.fillRect(0, 0, kW, kHeaderH);

        juce::ColourGradient line(kAccent,                0.0f, (float)kHeaderH,
                                   kAccent.withAlpha(0.0f), (float)kW, (float)kHeaderH, false);
        g.setGradientFill(line);
        g.fillRect(0.0f, (float)(kHeaderH - 1), (float)kW, 1.5f);

        g.setColour(kBorder.withAlpha(0.35f));
        for (int x = 260; x < kW - 20; x += 14)
            for (int y = 10; y < kHeaderH - 8; y += 10)
                g.fillEllipse((float)x, (float)y, 1.5f, 1.5f);

        const juce::Rectangle<float> badge(kW - 68.0f, 13.0f, 56.0f, 16.0f);
        g.setColour(kTrack);
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(kBorder);
        g.drawRoundedRectangle(badge, 4.0f, 0.5f);
        g.setFont(PluginFonts::get(9.0f));
        g.setColour(kMuted);
        g.drawText("v2.0", badge, juce::Justification::centred);
    }

    // ── Panels ────────────────────────────────────────────────────────────────
    const float top  = (float)lay.top;
    const float topH = (float)lay.topH;
    const float botY = (float)lay.botY;

    drawPanelBackground(g, { (float)lay.vizX,       top,  (float)lay.vizW,        topH }, true);
    drawPanelBackground(g, { (float)lay.ioX,        top,  (float)kIOW,            topH }, true);
    drawPanelBackground(g, { (float)lay.faderX,     top,  (float)lay.faderPanelW, topH }, true);
    drawPanelBackground(g, { (float)kPad,           botY, (float)kPhaseW,         (float)kBottomH });
    drawPanelBackground(g, { (float)lay.knobPanelX, botY, (float)lay.knobPanelW,  (float)kBottomH });

    // ── Section labels ────────────────────────────────────────────────────────
    drawSectionLabel(g, "VECTORSCOPE",
                     { (float)lay.vizX + 10.0f, top + 6.0f, 160.0f, 16.0f });
    drawSectionLabel(g, "PHASE CORRELATION",
                     { (float)kPad + 10.0f, botY + 6.0f, 200.0f, 16.0f });

    // ── Dividers ──────────────────────────────────────────────────────────────
    g.setColour(kBorder);
    const int colW = lay.knobPanelW / 4;
    for (int col = 1; col < 4; ++col)
    {
        float divX = (float)(lay.knobPanelX + colW * col);
        g.drawLine(divX, botY + 10.0f, divX, botY + kBottomH - 10.0f, 1.0f);
    }

    {
        float divX = (float)lay.ioX + (float)kIOW * 0.5f;
        g.drawLine(divX, top + 10.0f, divX, top + topH - 10.0f, 1.0f);
    }
}

//==============================================================================
void PracticeAudioProcessorEditor::resized()
{
    const auto lay = Layout::compute();

    // ── Header ────────────────────────────────────────────────────────────────
    pluginNameLabel.setBounds(20, 0, 300, kHeaderH);

    // ── Vectorscope ───────────────────────────────────────────────────────────
    visualizer.setBounds(lay.vizX + 10,
                          lay.top  + 24,
                          lay.vizW - 20,
                          lay.topH - 32);

    // ── I/O Gain faders ───────────────────────────────────────────────────────
    {
        const int faderW  = 60;
        const int textBoxH = 30;
        const int labelH   = 20;
        // Fill the panel: leave only padding for label above and textbox below
        const int faderH  = lay.topH - labelH - textBoxH - 20; // 20px total breathing
        const int topY    = lay.top + labelH + 10;              // 10px top breathing
        const int halfW   = kIOW / 2;

        const int inputX  = lay.ioX + (halfW - faderW) / 2;
        inputGainSlider.setBounds(inputX, topY, faderW, faderH);
        inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, textBoxH);

        const int outputX = lay.ioX + halfW + (halfW - faderW) / 2;
        outputGainSlider.setBounds(outputX, topY, faderW, faderH);
        outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, textBoxH);
    }

    // ── WIDTH fader ───────────────────────────────────────────────────────────
    {
        const int fW       = 60;
        const int textBoxH = 30;
        const int labelH   = 20;
        const int fH       = lay.topH - labelH - textBoxH - 20;
        const int topY     = lay.top + labelH + 10;
        const int fX       = lay.faderX + (lay.faderPanelW - fW) / 2;

        widthSlider.setBounds(fX, topY, fW, fH);
        widthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, textBoxH);

        widthLabel.setBounds(lay.faderX,
                             topY - labelH - 2,
                             lay.faderPanelW,
                             labelH);
    }

    // ── Phase correlation meter ───────────────────────────────────────────────
    // Compact insets to fit the shorter bottom panel
    phaseCorrelationMeter.setBounds(kPad + 10,
                                     lay.botY + 24,
                                     kPhaseW - 20,
                                     kBottomH - 32);

    // ── 4 rotary knobs — bottom-right panel ───────────────────────────────────
    {
        const int knobSize    = 80;
        const int textBoxH    = 40;
        const int labelH      = 20;
        const int sectionLblH = 40;   // space consumed by "PHASE CORRELATION" label at top
        const int innerPadBot = 8;    // breathing room from bottom border
        const int colW        = lay.knobPanelW / 4;

        // Total height of one knob's visual stack: label (above) + knob + textbox
        const int totalH = labelH + knobSize + textBoxH;

        // Available vertical space inside the panel, below the section label
        const int availableH = kBottomH - sectionLblH - innerPadBot;

        // Center the stack in that available space
        const int knobY = lay.botY + sectionLblH + (availableH - totalH) / 2 + labelH;

        std::array<juce::Slider*, 4> knobs = {
            &balanceSlider, &midSideSlider, &crossfeedSlider, &exciterEnhancerSlider
        };

        for (int i = 0; i < 4; ++i)
        {
            const int cx = lay.knobPanelX + colW * i + colW / 2;
            knobs[i]->setBounds(cx - knobSize / 2, knobY, knobSize, knobSize);
            knobs[i]->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, textBoxH);
        }
    }
}

//==============================================================================
void PracticeAudioProcessorEditor::timerCallback()
{
    std::vector<float> leftData, rightData;
    if (audioProcessor.getAudioDataForVisualizer(leftData, rightData))
    {
        visualizer.setAudioData(leftData, rightData);
        phaseCorrelationMeter.setAudioData(leftData, rightData);
    }
}
