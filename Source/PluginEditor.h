/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FFTDataGenerator.h"




template<typename PathType>
struct AnalyserPathGenerator
{
    /* converts 'render data[]' into a juce::path*/

    void generatePath(const std::vector<float>& renderData,
        juce::Rectangle<float> fftBounds,
        int fftSize,
        float binWidth,
        float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();

        int numBins = (int)fftSize / 2;

        PathType p;
        p.preallocateSpace(3 * (int)fftBounds.getWidth());

        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v,
                negativeInfinity, 0.f,
                float(bottom), top);
        };

        auto y = map(renderData[0]);

        jassert(!std::isnan(y) && !std::isinf(y));

        p.startNewSubPath(0, y);

        const int pathResolution = 2; // you can draw lineTo's every 'pathResolution' pixels

        for (int binNum = 1; binNum < numBins; binNum += pathResolution)
        {

            y = map(renderData[binNum]);

            jassert(!std::isnan(y) && !std::isinf(y));

            if (!std::isnan(y) && !std::isinf(y))
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                int binX = std::floor(normalizedBinX * width);
                p.lineTo(binX, y);

            }

        }
        pathFifo.push(p);

    }

    int getNumPathsAvailiable() const
    {
        return pathFifo.getNumAvailiableForReading();
    }


    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }

private:
    Fifo<PathType> pathFifo;

};
 



struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider) override;
    
    void drawToggleButton(juce::Graphics& g, 
        juce::ToggleButton& toggleButton, 
        bool shouldDrawButtonAsHighlighted, 
        bool shouldDrawButtonAsDown) override;
};

struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) 
        : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox),
        param(&rap),
        suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }


    struct LabelPos
    {
        float pos;
        juce::String label;
    };

    juce::Array<LabelPos> labels;

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;


};

struct PathProducer
{
    PathProducer(SingleChannelSampleFifo<SimpleEQAudioProcessor::BlockType>& scsf) :
        leftChannelFifo(&scsf)
    {
        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order8192);
        monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    }



    void process(juce::Rectangle<float> fftBounds, double sampleRate);
    juce::Path getPath() { return leftChannelFFTPath; }
private:
    SingleChannelSampleFifo<SimpleEQAudioProcessor::BlockType>* leftChannelFifo;

    juce::AudioBuffer<float> monoBuffer;

    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;

    AnalyserPathGenerator<juce::Path> pathProducer;

    juce::Path leftChannelFFTPath;
};


struct ResponseCurveComponent : juce::Component
    , juce::AudioProcessorParameter::Listener
    , juce::Timer
{
    ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent();

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override { }
    void timerCallback() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    void toggleAnalysisEnablement(bool enabled)
    {
        shouldShowFFTAnalysis = enabled;
    }
private:
    SimpleEQAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged{ false };

    MonoChain monoChain;

    void updateChain();

    juce::Image background;

    juce::Rectangle<int> getRenderArea();

    juce::Rectangle<int> getAnalysisArea();

    PathProducer leftPathProducer, rightPathProducer;

    bool shouldShowFFTAnalysis = true;
    

};


struct PowerButton : juce::ToggleButton { };
struct AnalyserButton : juce::ToggleButton 
{
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto insetRect = bounds.reduced(4);

        randomPath.clear();

        juce::Random r;

        // this is not ideal
        // we should cash some of the values
        // but we can always refactor later
        randomPath.startNewSubPath(insetRect.getX(),
            insetRect.getY() + insetRect.getHeight() * r.nextFloat());

        for (auto x = insetRect.getX() + 1; x < insetRect.getRight(); x += 2)
        {
            randomPath.lineTo(x,
                insetRect.getY() + insetRect.getHeight() * r.nextFloat());

        }
    }

    juce::Path randomPath;
};


//==============================================================================
/**
*/
class SimpleEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor&);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor& audioProcessor;


    // Create our custom Sliders
    RotarySliderWithLabels 
        peakFreqSlider,
        peakGainSlider,
        peakQSlider,
        lowCutFreqSlider,
        highCutFreqSlider,
        lowCutSlopeSlider,
        highCutSlopeSlider;

    ResponseCurveComponent responseCurveComponent;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment
        peakFreqSliderAttachment,
        peakGainSliderAttachment,
        peakQSliderAttachment,
        lowCutFreqSliderAttachment,
        highCutFreqSliderAttachment,
        lowCutSlopeSliderAttachment,
        highCutSlopeSliderAttachment;
       
    
    
    PowerButton lowcutBypassButton,
        highcutBypassButton,
        peakBypassButton;

    AnalyserButton analyserEnabledButton;
    
    using ButtonAttachment = APVTS::ButtonAttachment;

    ButtonAttachment 
        lowcutBypassButtonAttachment,
        highcutBypassButtonAttachment,
        peakBypassButtonAttachment,
        analyserEnabledButtonAttachment;
    
    // this function gives all our components 
    // in a vector so we can iterate easilly
    // through them
    std::vector<juce::Component*> getComps();


    LookAndFeel lnf;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
