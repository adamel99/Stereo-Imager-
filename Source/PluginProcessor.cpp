/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Param.h"
#include "VisualizerComponent.h"


//==============================================================================
PracticeAudioProcessor::PracticeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    width           = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::width));
    balance         = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::balance));
    inputGain       = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::inputGain));
    outputGain      = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParamIDs::outputGain));
    midSide         = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("midSide"));
    stereoSpread    = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("stereoSpread"));
    crossfeed       = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("crossfeed"));
    exciterEnhancer = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("exciterEnhancer"));
}

PracticeAudioProcessor::~PracticeAudioProcessor()
{
}

//==============================================================================
const juce::String PracticeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PracticeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PracticeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PracticeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PracticeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PracticeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PracticeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PracticeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PracticeAudioProcessor::getProgramName (int index)
{
    return {};
}

void PracticeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PracticeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // INTERVIEW NOTE: juce::dsp::ProcessSpec is how you tell any juce::dsp module
    // (Gain, DryWetMixer, filters, etc.) what sample rate / block size / channel
    // count to expect, so it can pre-allocate internal state sized correctly
    // exactly once here, rather than lazily on the audio thread (see the general
    // "never allocate in processBlock" RT-safety rule).
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dryWetMixer.prepare(spec);
    inputGainProcessor.prepare(spec);
    outputGainProcessor.prepare(spec);
    inputGainProcessor.setGainDecibels(inputGain->get());
    outputGainProcessor.setGainDecibels(outputGain->get());
    
    // Initialize audio data buffers for visualizer
    for (auto& buffer : audioDataBuffers)
        buffer.resize(samplesPerBlock * 2); // * 2 for stereo (left + right)
}

void PracticeAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PracticeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

// INTERVIEW NOTE: naming gotcha worth being ready to explain — this is called
// "width" in the UI/parameter, but mechanically it's driving a DryWetMixer's
// wet proportion, not a mid-side width coefficient (that's the SEPARATE
// `widthFactor` computed manually further down in processBlock and applied via
// mid/side math). So there are actually TWO different "width" mechanisms
// layered in this signal chain today — the DryWetMixer blend here (which mixes
// unprocessed dry input back in), and the mid/side widthFactor scaling below.
// Good to flag this as something you'd clean up/rename if reviewing your own code.
void PracticeAudioProcessor::updateStereoImagerParams()
{
    dryWetMixer.setWetMixProportion(width->get() * 0.01f);
    inputGainProcessor.setGainDecibels(inputGain->get());
    outputGainProcessor.setGainDecibels(outputGain->get());
}

void PracticeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    
    updateStereoImagerParams();
    
    // INTERVIEW NOTE: this is the dsp::Gain + dsp::DryWetMixer pattern for a
    // "dry/wet blended gain stage": pushDrySamples() snapshots the CURRENT block
    // (post input-gain, pre any further processing) as the "dry" reference, then
    // later mixWetSamples() blends that snapshot back in with whatever has been
    // computed into `context`'s output block by that point. Order matters here:
    // dry is captured right after input gain but BEFORE the manual mid/side/width/
    // balance/crossfeed/exciter math below, and mixWetSamples() is called
    // immediately after — meaning at the moment of mixing, the "wet" signal is
    // STILL just the input-gained dry signal (the width/balance/etc. math hasn't
    // run yet). So this DryWetMixer stage is effectively a no-op blend today
    // (dry == wet at mix time) — the actual width/imaging processing happens
    // entirely in the sample loop below, independently of this mixer. Worth being
    // able to trace signal flow like this out loud — "what does this actually do
    // vs. what does it look like it's supposed to do" is a very common code-review
    // interview exercise.
    inputGainProcessor.process(context);
    dryWetMixer.pushDrySamples(context.getInputBlock());
    dryWetMixer.mixWetSamples(context.getOutputBlock());
    outputGainProcessor.process(context);
    
    // Precompute factors
    float widthSliderValue = width->get();
    // INTERVIEW NOTE: piecewise width curve — center (50) = 1.0x side gain (unchanged
    // stereo image). Above 50, side gain scales UP linearly to widen the image;
    // below 50, side gain scales DOWN toward 0 (fully mono at slider = 0) — note the
    // asymmetric slopes (0.02 both ways here, but centered differently) mean the
    // widen side and narrow side don't necessarily feel equally "fast" across their
    // respective ranges; worth checking against the intended UX curve.
    float widthFactor = (widthSliderValue >= 50.0f) ? 1.0f + (widthSliderValue - 50.0f) * 0.02f : widthSliderValue * 0.02f;

    float balanceValue = balance->get();
    float midSideValue = midSide->get();
    // float depthFactor = depth->get() * 0.01f;
    float crossfeedFactor = crossfeed->get();
    float exciterEnhancerFactor = exciterEnhancer->get() * 0.01f;

    // INTERVIEW NOTE: everything below runs at AUDIO RATE with no smoothing on
    // widthFactor/balanceValue/midSideValue/crossfeedFactor/exciterEnhancerFactor —
    // they're read once per BLOCK (not per-sample) but applied instantaneously,
    // so an automated parameter change (or a fast knob turn) can produce a
    // block-boundary discontinuity ("zipper noise" — see the SmoothedValue notes
    // in the other plugin's PluginProcessor.cpp for the standard fix: ramp these
    // via juce::SmoothedValue instead of reading .get() once per block).
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float left = buffer.getSample(0, sample);
        float right = buffer.getSample(1, sample);

        // Width processing
        // INTERVIEW NOTE: classic Mid/Side (M/S) matrix — this is THE textbook
        // stereo-width technique and very likely to come up directly:
        //   mid  = (L + R) / 2   — the mono-compatible "common" content
        //   side = (L - R) / 2   — the stereo "difference" content
        // Scaling `side` by widthFactor and re-deriving L/R from (mid, side) is
        // exactly how a stereo widener/narrower works: widthFactor > 1 exaggerates
        // the difference between channels (wider image); widthFactor < 1 shrinks
        // it toward 0 (mono, since at side=0, L == R == mid).
        // The M/S <-> L/R transform is its own inverse (up to the scaling), which
        // is why this same mid/side decomposition is reused again a few lines
        // below for the midSide parameter.
        float mid = (left + right) * 0.5f;
        float side = (left - right) * 0.5f;
        left = mid + side * widthFactor;
        right = mid - side * widthFactor;

        // Balance processing
        // INTERVIEW NOTE: this is NOT a standard constant-power/equal-loudness pan
        // law (which would use e.g. sin/cos or sqrt curves so perceived loudness
        // stays constant as you pan). This is a simple linear "attenuate the
        // opposite channel" balance: at balanceValue > 0 (panned right), left is
        // scaled down by (1 - balanceValue) while right is untouched, and vice
        // versa. That means panning hard to one side doesn't boost the loud side,
        // it only cuts the quiet side — a common, perfectly valid "balance knob"
        // approach (as opposed to a "pan knob"), but worth knowing the distinction
        // and being able to name the alternative (equal-power panning) if asked.
        if (balanceValue <= 0.0f)
            left *= 1.0f;
        else
            left *= 1.0f - balanceValue;

        if (balanceValue >= 0.0f)
            right *= 1.0f;
        else
            right *= 1.0f + balanceValue;

        // Mid/side processing
        // INTERVIEW NOTE: a second, independent M/S decomposition — this one lets
        // midSideValue act as a mid/side BALANCE control: pushing it positive
        // boosts mid content into the side channel while shrinking side's own
        // contribution (mid += side*k; side -= side*k), which blends the two
        // components into each other rather than just scaling one. Because this
        // recomputes mid/side from the ALREADY width-and-balance-processed L/R
        // (not the original input), these three stages compound rather than
        // operate independently/orthogonally — moving one knob changes the
        // "reference point" the next knob's math starts from.
        mid = (left + right) * 0.5f;
        side = (left - right) * 0.5f;
        mid += side * midSideValue;
        side -= side * midSideValue;
        left = mid + side;
        right = mid - side;

        // Depth processing
        // left *= depthFactor;
        // right *= depthFactor;

        // Crossfeed processing (calculated outside the buffer setSample to avoid feedback)
        // INTERVIEW NOTE: "crossfeed" here just means bleeding a fraction of each
        // channel into the other — leftCrossfeed = left + k*right, and vice versa.
        // The comment about avoiding feedback is important and correct: both
        // leftCrossfeed and rightCrossfeed are computed from the SAME pre-crossfeed
        // left/right values, then written afterward. If instead you wrote
        // buffer.setSample(0, ...) first and then read buffer.getSample(1, ...)
        // for the right channel's crossfeed calc, you'd be feeding the ALREADY-
        // crossfed left channel into right's crossfeed — an unintended feedback
        // path compounding on itself. Computing both outputs from the same
        // snapshot of inputs (a "read-all-then-write-all" pattern) is the general
        // fix for this class of bug whenever a per-sample transform needs to mix
        // channels together.
        float leftCrossfeed = left + crossfeedFactor * right;
        float rightCrossfeed = right + crossfeedFactor * left;

        buffer.setSample(0, sample, leftCrossfeed);
        buffer.setSample(1, sample, rightCrossfeed);
    }
    
    // Exciter/enhancer processing
    // INTERVIEW NOTE: x + k*x^2 is a quadratic (2nd-order) waveshaper — this is
    // a genuinely useful thing to be able to explain analytically:
    //   - It's NOT odd-symmetric: f(-x) = -x + k*x^2 != -f(x) = -x - k*x^2 unless
    //     k = 0. Breaking odd symmetry means this generates EVEN harmonics
    //     (2nd, 4th, ...) on top of the fundamental — a classic "exciter" move,
    //     since even harmonics are perceived as adding brightness/presence
    //     without sounding as obviously "distorted" as odd-harmonic-heavy clipping.
    //   - x^2 is always >= 0, so adding k*x^2 also shifts the DC average of the
    //     signal upward (a rectification-like effect) — there's no DC-blocking
    //     high-pass filter after this stage, so that DC offset propagates
    //     downstream. Worth flagging as a real thing to fix (a one-pole HPF at
    //     ~5-20Hz after a nonlinearity like this is standard practice).
    //   - Unlike the tanh/softclip functions in a typical saturator, x^2 has NO
    //     asymptote — for x well above 1.0 (or negative excursions with large k)
    //     this can grow unbounded rather than gracefully saturating, so at high
    //     drive settings and hot input levels this can push levels far past unity
    //     with no ceiling. A production version would likely want to clamp/soft-
    //     clip the result.
    //   - Also note: this loop runs over totalNumInputChannels channels of the
    //     buffer AFTER the crossfeed stage above already wrote channels 0 and 1 —
    //     so the exciter is applied post-crossfeed, on the final stereo signal.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float sampleValue = channelData[sample];
            // Apply a simple harmonic excitation
            float excitedSample = sampleValue + exciterEnhancerFactor * sampleValue * sampleValue;
            channelData[sample] = excitedSample;
        }
    }
    
    // Store audio data for visualizer (thread-safe)
    // INTERVIEW NOTE: see the getAudioDataForVisualizer() note in PluginProcessor.h —
    // this is the "producer" side of that FIFO+lock hybrid. prepareToWrite(1, ...)
    // asks for exactly one slot in a 2-slot FIFO (double-buffered), writes the
    // full interleaved-by-half (left half then right half, not interleaved
    // per-sample) block into audioDataBuffers[start1], then finishedWrite signals
    // it's ready. Note this runs on the AUDIO THREAD and takes a CriticalSection
    // lock — see the header note on why that's a bit of a design smell alongside
    // an already-lock-free FIFO.
    if (buffer.getNumChannels() >= 2)
    {
        int start1, size1, start2, size2;
        audioDataFifo.prepareToWrite(1, start1, size1, start2, size2);
        
        if (size1 > 0)
        {
            const juce::ScopedLock sl(audioDataLock);
            auto& data = audioDataBuffers[start1];
            data.clear();
            
            // Store left channel
            data.insert(data.end(),
                       buffer.getReadPointer(0),
                       buffer.getReadPointer(0) + buffer.getNumSamples());
            
            // Store right channel
            data.insert(data.end(),
                       buffer.getReadPointer(1),
                       buffer.getReadPointer(1) + buffer.getNumSamples());
            
            audioDataFifo.finishedWrite(size1);
        }
    }
}


//==============================================================================
bool PracticeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PracticeAudioProcessor::createEditor()
{
    return new PracticeAudioProcessorEditor (*this);
}

//==============================================================================
void PracticeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void PracticeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateStereoImagerParams();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PracticeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percentFormat = [](float value, int /*maximumStringLength*/)
    {
        if (value < 10.0f)
            return juce::String(value, 2) + " %";
        else if (value < 100.0f)
            return juce::String(value, 1) + " %";
        else
            return juce::String(value, 0) + " %";
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ ParamIDs::width, 1 },
        ParamIDs::width,
        juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.01f, 1.0f },
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ ParamIDs::balance, 1 },
        ParamIDs::balance,
        juce::NormalisableRange<float>{ -1.0f, 1.0f, 0.01f, 1.0f },
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        nullptr,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ ParamIDs::inputGain, 1 },
        ParamIDs::inputGain,
        juce::NormalisableRange<float>{ -60.0f, 24.0f, 0.01f, 1.0f },  // Adjusted range
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 2) + " dB"; },  // This will convert to decibel string
        [](const juce::String& text) { return text.dropLastCharacters(3).getFloatValue(); }));  // This will convert from decibel string

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ ParamIDs::outputGain, 1 },
        ParamIDs::outputGain,
        juce::NormalisableRange<float>{ -60.0f, 24.0f, 0.01f, 1.0f },  // Adjusted range
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 2) + " dB"; },  // This will convert to decibel string
        [](const juce::String& text) { return text.dropLastCharacters(3).getFloatValue(); }));  // This will convert from decibel string

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "midSide", 1 },
        "Mid/Side",
        juce::NormalisableRange<float>{ -1.0f, 1.0f, 0.01f, 1.0f },
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        nullptr,
        nullptr));

//    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "depth", 1 },
//        "Depth",
//        juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.01f, 1.0f },
//        50.0f,
//        juce::String(),
//        juce::AudioProcessorParameter::genericParameter,
//        percentFormat,
//        nullptr));

    // New Parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "stereoSpread", 1 },
        "Stereo Spread",
        juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.01f, 1.0f },
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "crossfeed", 1 },
        "Crossfeed",
        juce::NormalisableRange<float>{ 0.0f, 1.0f, 0.01f, 1.0f },
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        nullptr,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "exciterEnhancer", 1 },
        "Exciter/Enhancer",
        juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.01f, 1.0f },
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    return layout;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PracticeAudioProcessor();
}
