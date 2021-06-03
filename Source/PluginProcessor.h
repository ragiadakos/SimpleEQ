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

// we need this enum class to easilly access the individual filters 
// inside the chain
enum ChainPossitions
{
    LowCut,
    Peak,
    HighCut
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
    
// Create namespace aliases
// this is a generic IIR filter class that can be used to represent 
// a single cut or peak filter
using Filter = juce::dsp::IIR::Filter<float>;

// the default juce IIR cut filters slope is 12 db/Oct 
// so we need 4 to acheive 48 db/Oct slope
// we will use the same processor chain for both cut filters 
// since they have the same architecture
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

// this helper function is used to update coefficients
// we make an alias to the type juce uses for the coeffs 
using Coefficients = Filter::CoefficientsPtr;

void updateCoefficients(Coefficients& old, const Coefficients& replacements);

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate);


template<int Index, typename ChainType, typename CoefficientType>
void update(ChainType& chain, CoefficientType& coefficients)
{
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
}



// we are not sure what typenames to use 
// for the parameters of our low cut update function 
// so we use a templated function
template<typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType& cut,
    const CoefficientType& cutCoefficients,
    const Slope& cutSlope)
{

    // compiles without the template keyword ??
    cut.template setBypassed<0>(true);
    cut.template setBypassed<1>(true);
    cut.template setBypassed<2>(true);
    cut.template setBypassed<3>(true);



    // now for each setting we will assign the respective set of coeffs and unbypass it
    // if we reverse the switch order we can leverage
    // case passthrough to eliminate code duplication
    // we dont break our switch statement
    switch (cutSlope)
    {
    case Slope_48:
        update<3>(cut, cutCoefficients);
    case Slope_36:
        update<2>(cut, cutCoefficients);
    case Slope_24:
        update<1>(cut, cutCoefficients);
    case Slope_12:
        update<0>(cut, cutCoefficients);
    }

}

// if we want to implement theese functions 
// in header files that are included in more
// than one place we need to use the inline 
// keyword otherwise the compiler will produce
// a definition for this function everywhere
// that this header file is included and the 
// linker will not now which version to use

inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
     return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
         sampleRate,
         (chainSettings.lowCutSlope + 1) * 2);
}
inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
        sampleRate,
        (chainSettings.highCutSlope + 1) * 2);
}

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
    // declare the 2 channels for stereo processing
    MonoChain leftChain, rightChain;
        
    void updatePeakFilter(const ChainSettings& chainSettings);
    void updateLowCutFilter(const ChainSettings& chainSettings);
    void updateHighCutFilter(const ChainSettings& chainSettings);

    void updateFilters();
        
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
