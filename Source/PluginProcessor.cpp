/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters(); // i missed that idk when it seemed to work fine thought

    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);

    // osc
    //osc.initialise([](float x) {return std::sin(x); });

    //spec.numChannels = getTotalNumOutputChannels();
    //osc.prepare(spec);
    //osc.setFrequency(200.f);
}

void SimpleEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SimpleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    updateFilters();




    // First we crate an audioBlock which grabs this buffer
    juce::dsp::AudioBlock<float> block(buffer);
    
    // osc
    //buffer.clear();
    //juce::dsp::ProcessContextReplacing<float> stereoContext(block);
    //osc.process(stereoContext);

    // Then we can use the helper function to extract the 2 channels individually
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    // create processing contexts for each channel
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // pass the audio context to each channel filter chain for processing
    leftChain.process(leftContext);
    rightChain.process(rightContext);


    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);

}


//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
    return new SimpleEQAudioProcessorEditor (*this);
    //return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);

}

void SimpleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }

}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibells = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQ = apvts.getRawParameterValue("Peak Q")->load();
    // when we changed the slope from int to Slope type we got a compiller error
    // we need to static_cast<Slope> the values before we assign them to correct it
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    return settings;
}

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    // calculate coefficients using the juce helper funcions
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
        chainSettings.peakFreq,
        chainSettings.peakQ,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibells));
}


void SimpleEQAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings)
{
    
    auto peakCoeficients = makePeakFilter(chainSettings, getSampleRate());

    // the coefficients object is a reference-counted wrapper around an array
    // that is allocated on the heap
    // we need to copy its values over so we need to dereference it
    // !!!allocating on the heap on an audio callback is bad!!!!
    // but we are gonna ignore that design flaw for now

    updateCoefficients(leftChain.get<ChainPossitions::Peak>().coefficients, peakCoeficients);
    updateCoefficients(rightChain.get<ChainPossitions::Peak>().coefficients, peakCoeficients);


}

void updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

void SimpleEQAudioProcessor::updateLowCutFilter(const ChainSettings& chainSettings)
{

    // this function will return an array with one coefficient object for every 2 filter orders
    // thus we need to provide it with 2, 4, 6, 8 for our 12, 24 ,36 ,48 db/Oct slopes
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());


    auto& leftLowCut = leftChain.get<ChainPossitions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPossitions::LowCut>();

    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);

}

void SimpleEQAudioProcessor::updateHighCutFilter(const ChainSettings& chainSettings)
{
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());

    auto& leftHighCut = leftChain.get<ChainPossitions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPossitions::HighCut>();

    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

void SimpleEQAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);

    updateLowCutFilter(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilter(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // slew rate doesnt work since we changed the sliders from CustomRotarySlider to RotarySliderWithLabel
    // so  i changed it back to 1
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", 
        "LowCut Freq", 
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 1.0f),
        20.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", 
        "HighCut Freq", 
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 1.0f),
        20000.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", 
        "Peak Freq", 
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 1.0f),
        750.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", 
        "Peak Gain", 
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f, 1.0f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Q", 
        "Peak Q", 
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f),
        1.0f));

    juce::StringArray strArr;
    for (int i = 0; i < 4; i++) 
    {
        juce::String str;
        str << (12 + i * 12);
        str << "db/Oct";
        strArr.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", 
        "LowCut Slope", 
        strArr, 0));
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", 
        "HighCut Slope", 
        strArr, 0));


    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQAudioProcessor();
}
