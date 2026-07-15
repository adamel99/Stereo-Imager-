// PluginFonts.h
#pragma once
#include <JuceHeader.h>

namespace PluginFonts
{
    // Returns the shared Fjalla One typeface (loaded once)
    inline juce::Typeface::Ptr getTypeface()
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
                                                                                BinaryData::FjallaOneRegular_ttf,
                                                                                BinaryData::FjallaOneRegular_ttfSize);
        return tf;
    }

    // Convenience: Fjalla One at any size
    inline juce::Font get(float height = 14.0f)
    {
        return juce::Font(getTypeface()).withHeight(height);
    }
}


