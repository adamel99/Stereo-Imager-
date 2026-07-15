/*
  ==============================================================================
    PhaseCorrelationMeter.cpp
  ==============================================================================
*/

#include <JuceHeader.h>
#include "PhaseCorrelationMeter.h"
#include "PluginFonts.h"

// ── Palette (mirrors PluginEditor) ────────────────────────────────────────────
namespace PCMColour
{
    static const juce::Colour bg      { 0xFF0E0E12 };
    static const juce::Colour panel   { 0xFF13131A };
    static const juce::Colour border  { 0xFF2A2A38 };
    static const juce::Colour track   { 0xFF1E1E2A };
    static const juce::Colour accent  { 0xFF4646C8 };
    static const juce::Colour accent2 { 0xFFF86F6F };   // coral – negative correlation
    static const juce::Colour text    { 0xFFE2E2EC };
    static const juce::Colour muted   { 0xFF5A5A72 };
}

//==============================================================================
PhaseCorrelationMeter::PhaseCorrelationMeter()
{
    startTimerHz(30);
}

PhaseCorrelationMeter::~PhaseCorrelationMeter()
{
    stopTimer();
}

//==============================================================================
void PhaseCorrelationMeter::setAudioData(const std::vector<float>& left,
                                         const std::vector<float>& right)
{
    const juce::ScopedLock sl(lock);
    leftChannelData  = left;
    rightChannelData = right;
    isActive = (!left.empty() && !right.empty());
}

//==============================================================================
// paint  –  three stacked zones:
//
//   [ bar meter + tick labels ]   ~28 px
//   [ large numeric readout   ]   ~52 px
//   [ sparkline history       ]   remainder
//==============================================================================
void PhaseCorrelationMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(PCMColour::panel);
    g.fillRoundedRectangle(bounds, 4.0f);

    if (!isActive)
    {
        g.setColour(PCMColour::muted);
        g.setFont(PluginFonts::get(12.0f));
        g.drawText("No Audio", bounds, juce::Justification::centred);
        return;
    }

    const float pad   = 8.0f;
    const float inner = bounds.getWidth() - pad * 2.0f;

    // ── 1. Bar meter (top) ────────────────────────────────────────────────────
    auto barZone = bounds.removeFromTop(36.0f);
    drawBarMeter(g, barZone.reduced(pad, 4.0f));

    // ── 2. Large numeric readout (middle) ─────────────────────────────────────
    auto readoutZone = bounds.removeFromTop(54.0f);
    drawReadout(g, readoutZone);

    // ── 3. Sparkline (bottom, whatever remains) ───────────────────────────────
    drawSparkline(g, bounds.reduced(pad, 4.0f));
}

//==============================================================================
void PhaseCorrelationMeter::drawBarMeter(juce::Graphics& g,
                                          juce::Rectangle<float> area)
{
    const float cornerR = area.getHeight() * 0.5f;   // pill shape

    // Track
    g.setColour(PCMColour::track);
    g.fillRoundedRectangle(area, cornerR);

    // ── Clip everything that follows to the pill shape ────────────────────────
    juce::Path clipPath;
    clipPath.addRoundedRectangle(area, cornerR);
    g.saveState();
    g.reduceClipRegion(clipPath);

    // Centre zero x
    const float zeroX = area.getX() + area.getWidth() * 0.5f;

    // Map correlation [-1..+1] → pixel position
    float valX = juce::jmap(phaseCorrelation, -1.0f, 1.0f,
                            area.getX(), area.getRight());
    valX = juce::jlimit(area.getX(), area.getRight(), valX);

    // Fill from zero toward value
    if (phaseCorrelation >= 0.0f)
    {
        auto fill = juce::Rectangle<float>(zeroX, area.getY(),
                                            valX - zeroX, area.getHeight());
        g.setColour(PCMColour::accent);
        g.fillRect(fill);
    }
    else
    {
        auto fill = juce::Rectangle<float>(valX, area.getY(),
                                            zeroX - valX, area.getHeight());
        g.setColour(PCMColour::accent2);
        g.fillRect(fill);
    }

    // Needle tick at current value
    g.setColour(PCMColour::text);
    g.fillRect(valX - 1.0f, area.getY(), 2.0f, area.getHeight());

    // Centre zero mark
    g.setColour(PCMColour::muted);
    g.fillRect(zeroX - 0.5f, area.getY(), 1.0f, area.getHeight());

    // ── Restore clip before drawing border/labels ─────────────────────────────
    g.restoreState();

    // Border (drawn outside the clip so it sits clean on top)
    g.setColour(PCMColour::border);
    g.drawRoundedRectangle(area, cornerR, 0.75f);

    // Tick labels: -1  |  0  |  +1
    const float labelY = area.getBottom() + 11.0f;
    g.setFont(PluginFonts::get(9.0f));
    g.setColour(PCMColour::muted);
    g.drawText("-1", juce::Rectangle<float>(area.getX() - 8.0f, labelY, 24.0f, 20.0f),
               juce::Justification::centred);
    g.drawText("+1", juce::Rectangle<float>(area.getRight() - 16.0f, labelY, 24.0f, 20.0f),
               juce::Justification::centred);
}

//==============================================================================
void PhaseCorrelationMeter::drawReadout(juce::Graphics& g,
                                         juce::Rectangle<float> area)
{
    // Numeric value – large
    const juce::String numStr = juce::String(phaseCorrelation, 2)
                                    .paddedLeft(' ', 5);   // e.g. "+0.68", "-0.12"
    // Prepend explicit + sign for positive values
    const juce::String display = (phaseCorrelation >= 0.0f)
                                    ? "+" + juce::String(phaseCorrelation, 2)
                                    : juce::String(phaseCorrelation, 2);

    // Pick colour: teal ≥ 0, coral < 0, red if |corr| < 0.3 (phase issues)
    juce::Colour numColour;
    if      (phaseCorrelation >= 0.5f)  numColour = PCMColour::accent;
    else if (phaseCorrelation >= 0.0f)  numColour = PCMColour::accent.interpolatedWith(PCMColour::text, 0.4f);
    else                                numColour = PCMColour::accent2;

    g.setFont(PluginFonts::get(28.0f));
    g.setColour(numColour);
    g.drawText(display,
               area.withTrimmedBottom(16.0f),
               juce::Justification::centred);

    // Status string underneath
    juce::String status;
    if      (phaseCorrelation >  0.7f)  status = "CORRELATED";
    else if (phaseCorrelation >  0.2f)  status = "PARTIAL";
    else if (phaseCorrelation > -0.2f)  status = "UNCORRELATED";
    else                                status = "OUT OF PHASE";

    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.setColour(PCMColour::muted);
    g.drawText(status,
               area.withTrimmedTop(area.getHeight() - 18.0f),
               juce::Justification::centred);
}

//==============================================================================
void PhaseCorrelationMeter::drawSparkline(juce::Graphics& g,
                                           juce::Rectangle<float> area)
{
    if (phaseHistory.size() < 2)
        return;

    // Zero line
    const float zeroY = area.getCentreY();
    g.setColour(PCMColour::border);
    g.drawHorizontalLine((int)zeroY, area.getX(), area.getRight());

    // Path
    juce::Path path;
    const float lastIdx = static_cast<float>(phaseHistory.size() - 1);

    for (size_t i = 0; i < phaseHistory.size(); ++i)
    {
        float x = juce::jmap(static_cast<float>(i), 0.0f, lastIdx,
                             area.getX(), area.getRight());
        float y = juce::jmap(juce::jlimit(-1.0f, 1.0f, phaseHistory[i]),
                             -1.0f, 1.0f,
                             area.getBottom(), area.getY());
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    // Stroke colour: teal if last value positive, coral if negative
    juce::Colour lineCol = (phaseCorrelation >= 0.0f)
                               ? PCMColour::accent.withAlpha(0.75f)
                               : PCMColour::accent2.withAlpha(0.75f);
    g.setColour(lineCol);
    g.strokePath(path, juce::PathStrokeType(1.5f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Filled area under curve (subtle)
    juce::Path filled = path;
    filled.lineTo(area.getRight(), zeroY);
    filled.lineTo(area.getX(),     zeroY);
    filled.closeSubPath();
    g.setColour(lineCol.withAlpha(0.08f));
    g.fillPath(filled);
}

//==============================================================================
void PhaseCorrelationMeter::resized() {}

//==============================================================================
void PhaseCorrelationMeter::timerCallback()
{
    const juce::ScopedLock sl(lock);

    if (!isActive)
        return;

    phaseCorrelation = calculatePhaseCorrelation();
    phaseHistory.push_back(phaseCorrelation);

    if (phaseHistory.size() > 120)
        phaseHistory.erase(phaseHistory.begin());

    repaint();
}

//==============================================================================
float PhaseCorrelationMeter::calculatePhaseCorrelation()
{
    if (leftChannelData.size() != rightChannelData.size() || leftChannelData.empty())
        return 0.0f;

    const size_t step = std::max<size_t>(leftChannelData.size() / 1000, 1);
    float sumProduct = 0.0f, sumLeft = 0.0f, sumRight = 0.0f;

    for (size_t i = 0; i < leftChannelData.size(); i += step)
    {
        sumProduct += leftChannelData[i]  * rightChannelData[i];
        sumLeft    += leftChannelData[i]  * leftChannelData[i];
        sumRight   += rightChannelData[i] * rightChannelData[i];
    }

    const float denom = std::sqrt(sumLeft) * std::sqrt(sumRight);
    if (denom < 1e-10f)
        return 0.0f;

    return juce::jlimit(-1.0f, 1.0f, sumProduct / denom);
}
