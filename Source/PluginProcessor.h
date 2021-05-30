/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// this enum helps with setting the coefficients for the filters
enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48,
};


// struct that contains the filter parameters
struct ChainSettings
{
    float peakFreq{ 0 }, peakGainInDecibells{ 0 }, peakQ{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    
    // Instead of integers the slopes are now expressed in Slope type objects
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };

};

// function that return the parameters in a ChainSettings struct
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);


//==============================================================================
/**
*/
class SimpleEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

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

    // Add Parameters
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    // Create namespace aliases
    // this is a generic IIR filter class that can be used to represent a single cut or peak filter
    using Filter = juce::dsp::IIR::Filter<float>;

    // the default juce IIR cut filters slope is 12 db/Oct so we need 4 to acheive 48 db/Oct slope
    // we will use the same processor chain for both cut filters since they have the same architecture
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    // declare the 2 channels for stereo processing
    MonoChain leftChain, rightChain;

    // we need this enum class to easilly access the individual filters inside the chain
    enum ChainPossitions
    {
        LowCut,
        Peak,
        HighCut
    };

    void updatePeakFilter(const ChainSettings& chainSettings);
    
    // this helper function is used to update coefficients
    // we make an alias to the type juce uses for the coeffs 
    using Coefficients = Filter::CoefficientsPtr;
    // we use static for convenience instead of scrolling up to make a free function
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);
    
    
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
