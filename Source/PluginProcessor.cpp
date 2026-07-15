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
    
    inputGainProcessor.process(context);
    dryWetMixer.pushDrySamples(context.getInputBlock());
    dryWetMixer.mixWetSamples(context.getOutputBlock());
    outputGainProcessor.process(context);
    
    // Precompute factors
    float widthSliderValue = width->get();
    float widthFactor = (widthSliderValue >= 50.0f) ? 1.0f + (widthSliderValue - 50.0f) * 0.02f : widthSliderValue * 0.02f;

    float balanceValue = balance->get();
    float midSideValue = midSide->get();
    // float depthFactor = depth->get() * 0.01f;
    float crossfeedFactor = crossfeed->get();
    float exciterEnhancerFactor = exciterEnhancer->get() * 0.01f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float left = buffer.getSample(0, sample);
        float right = buffer.getSample(1, sample);

        // Width processing
        float mid = (left + right) * 0.5f;
        float side = (left - right) * 0.5f;
        left = mid + side * widthFactor;
        right = mid - side * widthFactor;

        // Balance processing
        if (balanceValue <= 0.0f)
            left *= 1.0f;
        else
            left *= 1.0f - balanceValue;

        if (balanceValue >= 0.0f)
            right *= 1.0f;
        else
            right *= 1.0f + balanceValue;

        // Mid/side processing
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
        float leftCrossfeed = left + crossfeedFactor * right;
        float rightCrossfeed = right + crossfeedFactor * left;

        buffer.setSample(0, sample, leftCrossfeed);
        buffer.setSample(1, sample, rightCrossfeed);
    }
    
    // Exciter/enhancer processing
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
