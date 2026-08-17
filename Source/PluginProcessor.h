/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "VisualizerComponent.h"

//==============================================================================
/**
*/
class PracticeAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PracticeAudioProcessor();
    ~PracticeAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };
    
    // Public getter functions for the parameters
    juce::AudioParameterFloat* getWidthParam()           const { return width; }
    juce::AudioParameterFloat* getBalanceParam()         const { return balance; }
    juce::AudioParameterFloat* getInputGainParam()       const { return inputGain; }
    juce::AudioParameterFloat* getOutputGainParam()      const { return outputGain; }
    juce::AudioParameterFloat* getMidSideParam()         const { return midSide; }
    juce::AudioParameterFloat* getCrossfeedParam()       const { return crossfeed; }
    juce::AudioParameterFloat* getExciterEnhancerParam() const { return exciterEnhancer; }
    juce::AudioParameterFloat* getStereoSpreadParam()    const { return stereoSpread; }

    // Thread-safe method to get audio data for visualizer
    // INTERVIEW NOTE: this is the UI-thread ("consumer") half of a producer/consumer
    // handoff from processBlock (see the matching prepareToWrite in the .cpp).
    // Worth flagging out loud in an interview: this mixes a LOCK-FREE juce::AbstractFifo
    // (whose whole purpose is to avoid ever blocking the audio thread) with a
    // juce::CriticalSection (a real OS mutex) taken on BOTH sides. That's a design
    // smell — if you already have a correct lock-free index scheme from the FIFO,
    // adding a mutex around the payload copy reintroduces exactly the audio-thread
    // blocking risk the FIFO was meant to eliminate (priority inversion, glitches
    // under lock contention). A more consistent design would either (a) drop the
    // CriticalSection and rely purely on the FIFO's happens-before guarantees since
    // each slot index is only touched by one writer/one reader at a time, or (b) drop
    // the FIFO and use a triple-buffer/atomic-pointer-swap scheme instead. Good thing
    // to mention as "I'd tighten this up" if asked to critique your own code.
    bool getAudioDataForVisualizer(std::vector<float>& leftData, std::vector<float>& rightData)
    {
        int start1, size1, start2, size2;
        audioDataFifo.prepareToRead(1, start1, size1, start2, size2);
        
        if (size1 > 0)
        {
            const juce::ScopedLock sl(audioDataLock);
            auto& data = audioDataBuffers[start1];
            int samplesPerChannel = static_cast<int>(data.size()) / 2;
            
            leftData.assign(data.begin(), data.begin() + samplesPerChannel);
            rightData.assign(data.begin() + samplesPerChannel, data.end());
            
            audioDataFifo.finishedRead(size1);
            return true;
        }
        return false;
    }

private:
    juce::AudioParameterFloat* width           = nullptr;
    juce::AudioParameterFloat* balance         = nullptr;
    juce::AudioParameterFloat* inputGain       = nullptr;
    juce::AudioParameterFloat* outputGain      = nullptr;
    juce::AudioParameterFloat* midSide         = nullptr;
    juce::AudioParameterFloat* stereoSpread    = nullptr;
    juce::AudioParameterFloat* crossfeed       = nullptr;
    juce::AudioParameterFloat* exciterEnhancer = nullptr;

    void updateStereoImagerParams();

    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::dsp::Gain<float> inputGainProcessor;
    juce::dsp::Gain<float> outputGainProcessor;
    
    // Thread-safe audio data passing for visualizer
    juce::AbstractFifo audioDataFifo { 2 };
    std::array<std::vector<float>, 2> audioDataBuffers;
    juce::CriticalSection audioDataLock;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeAudioProcessor)
};
