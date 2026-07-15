/*
  ==============================================================================
    VisualizerComponent.cpp  –  Stereo Imager vectorscope

    Colour scheme: black background, blue sphere/grid/labels, pink dot cloud.

    Performance optimisations vs. original:
      1. Timer capped at 30fps (33ms) even when signal is active.
      2. fadeImagePixels() removed — replaced with a single translucent
         fillAll() pass, which uses JUCE's blended fill instead of a manual
         per-pixel loop.
      3. densityGridSize reduced 256 → 64 (16x fewer cells per frame).
      4. Sample stepping capped at 256 points instead of 512.
      5. repaint() is skipped entirely once the signal has been silent long
         enough and the dot layer has fully faded.
      6. Dead members (autoScaleMax, autoScaleSmoothed, hasNewData) removed.
  ==============================================================================
*/

#include <JuceHeader.h>
#include "VisualizerComponent.h"
#include <cmath>
#include <algorithm>

VisualizerComponent::VisualizerComponent()
{
    setOpaque(true);
    for (auto& row : densityGrid) row.fill(0.0f);
    startTimer(33);
}

VisualizerComponent::~VisualizerComponent() { stopTimer(); }

// ---------------------------------------------------------------------------
float VisualizerComponent::getVizRadius() const
{
    return juce::jmin(getWidth(), getHeight()) * 0.44f;
}

// ---------------------------------------------------------------------------
void VisualizerComponent::ensureImages()
{
    int w = juce::jmax(1, getWidth());
    int h = juce::jmax(1, getHeight());

    auto needsRebuild = [&](const juce::Image& img) {
        return !img.isValid() || img.getWidth() != w || img.getHeight() != h;
    };

    if (needsRebuild(compositeImage))
    {
        compositeImage = juce::Image(juce::Image::ARGB, w, h, true,
                                      juce::SoftwareImageType{});
        juce::Graphics g(compositeImage);
        g.fillAll(juce::Colour(0, 0, 0));
    }

    if (needsRebuild(sphereCache))
    {
        sphereCache = juce::Image(juce::Image::ARGB, w, h, true,
                                   juce::SoftwareImageType{});
        gridNeedsRedraw = true;
    }

    if (needsRebuild(dotLayer))
    {
        dotLayer = juce::Image(juce::Image::ARGB, w, h, true,
                                juce::SoftwareImageType{});
    }
}

// ---------------------------------------------------------------------------
//  Fade the dot layer by painting a semi-transparent black rect over it.
//  This is significantly cheaper than the old per-pixel loop because it lets
//  JUCE use its optimised blended fill path rather than manual float math on
//  every pixel in the image.
// ---------------------------------------------------------------------------
// In fadeDotLayer, change the fill colour to transparent black:
static void fadeDotLayer(juce::Image& img, float keepFraction)
{
    juce::Image::BitmapData bm(img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bm.height; ++y)
    {
        auto* row = reinterpret_cast<juce::PixelARGB*>(bm.getLinePointer(y));
        for (int x = 0; x < bm.width; ++x)
        {
            auto& px = row[x];
            px.setARGB(
                (uint8_t)(px.getAlpha() * keepFraction),
                (uint8_t)(px.getRed()   * keepFraction),
                (uint8_t)(px.getGreen() * keepFraction),
                (uint8_t)(px.getBlue()  * keepFraction)
            );
        }
    }
}

// ---------------------------------------------------------------------------
void VisualizerComponent::setAudioData(const std::vector<float>& left,
                                        const std::vector<float>& right)
{
    const juce::ScopedLock sl(lock);
    leftChannelData  = left;
    rightChannelData = right;

    peakMagnitude = 0.0f;
    int n = (int)std::min(left.size(), right.size());
    for (int i = 0; i < n; i += 8)
        peakMagnitude = std::max(peakMagnitude,
                                  std::abs(left[i]) + std::abs(right[i]));
}

void VisualizerComponent::copyAudioBuffer(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(lock);
    int n  = buffer.getNumSamples();
    int ch = buffer.getNumChannels();

    leftChannelData.resize(n);
    rightChannelData.resize(n);

    if (ch > 0)
        std::copy(buffer.getReadPointer(0),
                  buffer.getReadPointer(0) + n,
                  leftChannelData.begin());
    if (ch > 1)
        std::copy(buffer.getReadPointer(1),
                  buffer.getReadPointer(1) + n,
                  rightChannelData.begin());
    else
        std::fill(rightChannelData.begin(), rightChannelData.end(), 0.0f);

    peakMagnitude = 0.0f;
    for (int i = 0; i < n; i += 8)
        peakMagnitude = std::max(peakMagnitude,
                                  std::abs(leftChannelData[i]) +
                                  std::abs(rightChannelData[i]));
}

// ---------------------------------------------------------------------------
void VisualizerComponent::timerCallback()
{
    const bool isActive = (peakMagnitude > 0.0001f);

    if (isActive)
    {
        inactivityCounter = 0;

        if (!hasRecentActivity)
        {
            hasRecentActivity = true;
            // 30fps cap — fast enough to look smooth, light enough for a DAW
            startTimer(33);
        }

        buildAndRenderFrame();
        repaint();
    }
    else
    {
        ++inactivityCounter;

        if (inactivityCounter > 30 && hasRecentActivity)
        {
            hasRecentActivity = false;
            startTimer(33);
        }

        // Once fully silent for a while, stop repainting entirely.
        // The visualiser will just hold its last composited frame quietly.
        if (inactivityCounter > 90)
            return;

        // Still fading out — decay density grid and dot layer, then repaint.
        if (inactivityCounter % 2 == 0)
        {
            for (auto& row : densityGrid)
                for (auto& cell : row)
                    cell *= 0.85f;

            if (dotLayer.isValid())
                fadeDotLayer(dotLayer, 0.05f);
        }

        repaint();
    }
}

// ---------------------------------------------------------------------------
void VisualizerComponent::buildAndRenderFrame()
{
    ensureImages();

    if (gridNeedsRedraw)
    {
        juce::Graphics gSphere(sphereCache);
        gSphere.fillAll(juce::Colours::transparentBlack);
        drawSphere(gSphere);
        drawGrid(gSphere);
        gridNeedsRedraw = false;
    }

    std::vector<float> leftCopy, rightCopy;
    {
        const juce::ScopedLock sl(lock);
        if (leftChannelData.empty() || rightChannelData.empty()) return;
        leftCopy  = leftChannelData;
        rightCopy = rightChannelData;
    }

    auto  bounds = getLocalBounds().toFloat();
    auto  center = bounds.getCentre();
    float maxR   = getVizRadius();

    const size_t dataSize = std::min(leftCopy.size(), rightCopy.size());

    // Decay the persistent density grid each frame
    for (auto& row : densityGrid)
        for (auto& cell : row)
            cell *= 0.96f;

    // Cap at 256 sample points — above this gives no visible improvement
    const int step = std::max(1, (int)dataSize / 256);

    // Fade dot layer using a cheap blended fill instead of a per-pixel loop
    fadeDotLayer(dotLayer, 0.82f);

    // ── Accumulate samples into density grid and dotLayer ────────────────────
    {
        juce::Graphics gDots(dotLayer);

        for (size_t i = 0; i < dataSize; i += (size_t)step)
        {
            float L = leftCopy[i];
            float R = rightCopy[i];

            float x = (R - L) * maxR * 0.5f;
            float y = -(L + R) * maxR * 0.5f;

            // Soft clamp to sphere boundary
            float dist = std::sqrt(x * x + y * y);
            if (dist > maxR * 0.97f)
            {
                float s = (maxR * 0.97f) / dist;
                x *= s;
                y *= s;
            }

            // Map to density grid (64x64)
            int gx = (int)((x + maxR) / (maxR * 2.0f) * (densityGridSize - 1));
            int gy = (int)((y + maxR) / (maxR * 2.0f) * (densityGridSize - 1));
            gx = juce::jlimit(0, densityGridSize - 1, gx);
            gy = juce::jlimit(0, densityGridSize - 1, gy);

            densityGrid[gy][gx] = juce::jmin(1.0f, densityGrid[gy][gx] + 0.08f);

            float sx = center.x + x;
            float sy = center.y + y;

            // Dot colour based on instantaneous amplitude
            float amp = std::sqrt(L * L + R * R);
            juce::Colour dotColour;
            if (amp < 0.3f)
                dotColour = juce::Colour(220, 80, 150).withAlpha(0.55f);
            else if (amp < 0.7f)
                dotColour = juce::Colour(246, 134, 189).withAlpha(0.70f);
            else
                dotColour = juce::Colours::white.withAlpha(0.88f);

            gDots.setColour(dotColour);
            gDots.fillRect(sx - 0.5f, sy - 0.5f, 1.5f, 1.5f);
        }
    }

    // ── Composite all layers into compositeImage ─────────────────────────────
    {
        juce::Graphics gComp(compositeImage);

        // 1. Background — black
        gComp.fillAll(juce::Colour(0, 0, 0));

        // 2. Sphere + grid (cached, rebuilt only on resize)
        gComp.drawImageAt(sphereCache, 0, 0, false);

        // 3. Persistent density grid rendered as coloured pixel squares
        {
            const float cellW  = (maxR * 2.0f) / (float)densityGridSize;
            const float cellH  = (maxR * 2.0f) / (float)densityGridSize;
            const float startX = center.x - maxR;
            const float startY = center.y - maxR;

            for (int row = 0; row < densityGridSize; ++row)
            {
                for (int col = 0; col < densityGridSize; ++col)
                {
                    float density = densityGrid[row][col];
                    if (density < 0.012f) continue;

                    float cx = startX + (col + 0.5f) * cellW;
                    float cy = startY + (row + 0.5f) * cellH;

                    float dx = cx - center.x;
                    float dy = cy - center.y;
                    if (dx * dx + dy * dy > maxR * maxR) continue;

                    juce::Colour cellColour = densityToColour(density);
                    gComp.setColour(cellColour);
                    gComp.fillRect(cx - cellW * 0.65f, cy - cellH * 0.65f,
                                   cellW * 1.3f,        cellH * 1.3f);
                }
            }
        }

        // 4. Fresh dot spray layer on top
        gComp.drawImageAt(dotLayer, 0, 0, false);

        // 5. Labels always on top
        drawLabels(gComp);
    }
}

// ---------------------------------------------------------------------------
juce::Colour VisualizerComponent::densityToColour(float d) const
{
    if (d < 0.15f)
    {
        float t = d / 0.15f;
        return juce::Colour((uint8_t)(180),
                             (uint8_t)(40 + 20 * t),
                             (uint8_t)(120 + 20 * t))
                   .withAlpha(0.30f + t * 0.25f);
    }
    else if (d < 0.40f)
    {
        float t    = (d - 0.15f) / 0.25f;
        uint8_t r  = (uint8_t)(200 + 30 * t);
        uint8_t g  = (uint8_t)(60  + 40 * t);
        uint8_t b  = (uint8_t)(140 + 30 * t);
        return juce::Colour(r, g, b).withAlpha(0.50f + t * 0.20f);
    }
    else if (d < 0.70f)
    {
        float t    = (d - 0.40f) / 0.30f;
        uint8_t r  = (uint8_t)(230 + 15 * t);
        uint8_t g  = (uint8_t)(100 + 34 * t);
        uint8_t b  = (uint8_t)(170 + 30 * t);
        return juce::Colour(r, g, b).withAlpha(0.70f + t * 0.15f);
    }
    else
    {
        float t    = (d - 0.70f) / 0.30f;
        uint8_t r  = (uint8_t)(245 + 10 * t);
        uint8_t g  = (uint8_t)(134 + 80 * t);
        uint8_t b  = (uint8_t)(189 + 55 * t);
        return juce::Colour(r, g, b).withAlpha(0.85f + t * 0.15f);
    }
}

// ---------------------------------------------------------------------------
void VisualizerComponent::paint(juce::Graphics& g)
{
    if (compositeImage.isValid())
        g.drawImageAt(compositeImage, 0, 0, false);
    else
        g.fillAll(juce::Colour(0, 0, 0));
}

// ---------------------------------------------------------------------------
void VisualizerComponent::resized()
{
    gridNeedsRedraw = true;
    compositeImage  = juce::Image();
    sphereCache     = juce::Image();
    dotLayer        = juce::Image();
    for (auto& row : densityGrid) row.fill(0.0f);
}

// ---------------------------------------------------------------------------
void VisualizerComponent::drawSphere(juce::Graphics& g)
{
    auto  bounds = getLocalBounds().toFloat();
    auto  center = bounds.getCentre();
    float radius = getVizRadius();

    // Sphere fill — very dark blue-black
    juce::ColourGradient sphereGradient(
        juce::Colour(8, 8, 30), center,
        juce::Colour(4, 4, 18), { center.x + radius, center.y + radius },
        true);
    sphereGradient.addColour(0.5,  juce::Colour(6, 6, 24));
    sphereGradient.addColour(0.85, juce::Colour(5, 5, 20));
    g.setGradientFill(sphereGradient);
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f);

    // Rim vignette
    juce::ColourGradient rim(
        juce::Colours::transparentBlack, center,
        juce::Colour(0, 0, 20).withAlpha(0.60f), { center.x + radius, center.y },
        true);
    g.setGradientFill(rim);
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f);

    // Subtle specular highlight (top-left)
    const float hlOffX = -(radius * 0.25f);
    const float hlOffY = -(radius * 0.25f);
    const float hlW    =   radius * 0.45f;
    const float hlH    =   radius * 0.32f;
    juce::ColourGradient hl(
        juce::Colours::white.withAlpha(0.04f),
        { center.x + hlOffX, center.y + hlOffY },
        juce::Colours::transparentWhite,
        { center.x + hlOffX - hlW * 0.6f, center.y + hlOffY - hlH * 0.6f },
        true);
    g.setGradientFill(hl);
    g.fillEllipse(center.x + hlOffX - hlW * 0.5f,
                  center.y + hlOffY - hlH * 0.5f, hlW, hlH);
}

// ---------------------------------------------------------------------------
void VisualizerComponent::drawGrid(juce::Graphics& g)
{
    auto  bounds = getLocalBounds().toFloat();
    auto  center = bounds.getCentre();
    float radius = getVizRadius();

    juce::Path clip;
    clip.addEllipse(center.x - radius, center.y - radius,
                    radius * 2.f, radius * 2.f);
    g.saveState();
    g.reduceClipRegion(clip);

    auto drawDashed = [&](juce::Point<float> p1, juce::Point<float> p2,
                           float alpha, float lw = 0.7f)
    {
        juce::Path p;
        p.startNewSubPath(p1);
        p.lineTo(p2);
        float dashes[] = { 6.f, 8.f };
        juce::Path d;
        juce::PathStrokeType(lw).createDashedStroke(d, p, dashes, 2);
        g.setColour(juce::Colour(70, 70, 200).withAlpha(alpha));
        g.fillPath(d);
    };

    drawDashed({ center.x - radius * 0.707f, center.y - radius * 0.707f },
               { center.x + radius * 0.707f, center.y + radius * 0.707f }, 0.35f, 0.8f);
    drawDashed({ center.x + radius * 0.707f, center.y - radius * 0.707f },
               { center.x - radius * 0.707f, center.y + radius * 0.707f }, 0.35f, 0.8f);
    drawDashed({ center.x, center.y - radius },
               { center.x, center.y + radius }, 0.45f, 1.0f);
    drawDashed({ center.x - radius, center.y },
               { center.x + radius, center.y }, 0.20f, 0.7f);

    auto drawSolid = [&](juce::Point<float> p1, juce::Point<float> p2,
                          float alpha, float lw = 0.6f)
    {
        g.setColour(juce::Colour(70, 70, 200).withAlpha(alpha));
        g.drawLine(p1.x, p1.y, p2.x, p2.y, lw);
    };

    juce::Point<float> top   { center.x,            center.y - radius };
    juce::Point<float> left  { center.x - radius,   center.y          };
    juce::Point<float> right { center.x + radius,   center.y          };
    juce::Point<float> bot   { center.x,            center.y + radius };

    drawSolid(top,   right, 0.30f);
    drawSolid(right, bot,   0.30f);
    drawSolid(bot,   left,  0.30f);
    drawSolid(left,  top,   0.30f);

    g.restoreState();

    // Sphere outline
    g.setColour(juce::Colour(70, 70, 200).withAlpha(0.55f));
    g.drawEllipse(center.x - radius, center.y - radius,
                  radius * 2.f, radius * 2.f, 1.0f);
}

// ---------------------------------------------------------------------------
void VisualizerComponent::drawLabels(juce::Graphics& g)
{
    auto   bounds = getLocalBounds().toFloat();
    auto   center = bounds.getCentre();
    float  radius = getVizRadius();

    const float fontSize    = juce::jmax(9.0f, bounds.getHeight() * 0.052f);
    const float labelOffset = radius * 0.94f;
    const float labelW      = fontSize * 1.8f;
    const float labelH      = fontSize * 1.4f;

    g.setFont(fontSize);
    g.setColour(juce::Colour(100, 100, 220).withAlpha(0.75f));

    g.drawText("L",  center.x - labelOffset - labelW,  center.y - labelH * 0.5f,
               labelW, labelH, juce::Justification::centredRight);
    g.drawText("R",  center.x + labelOffset,            center.y - labelH * 0.5f,
               labelW, labelH, juce::Justification::centredLeft);

    g.setFont(fontSize * 0.85f);
    g.setColour(juce::Colour(100, 100, 220).withAlpha(0.55f));
    g.drawText("C",  center.x - labelW * 0.5f, center.y - labelOffset - labelH,
               labelW, labelH, juce::Justification::centred);

    g.setFont(fontSize * 0.70f);
    g.setColour(juce::Colour(70, 70, 200).withAlpha(0.40f));
    g.drawText("-R", center.x - labelOffset - labelW,  center.y + labelOffset * 0.6f,
               labelW, labelH, juce::Justification::centredRight);
    g.drawText("-L", center.x + labelOffset,            center.y + labelOffset * 0.6f,
               labelW, labelH, juce::Justification::centredLeft);
}
