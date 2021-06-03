/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, 
    int y, 
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider&)
{
    using namespace juce;

    // fill a circle
    // add border
    // convert value to radians
    // create rectangle
    // rotate it to the value radians

    auto bounds = Rectangle<float>(x, y, width, height);

    
    
    g.setColour(Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);

    g.setColour(Colour(255u, 154u, 1u));
    g.drawEllipse(bounds, 1.f);

    auto center = bounds.getCentre();


    Path p;

    Rectangle<float> r;
    r.setLeft(center.getX() - 2);
    r.setRight(center.getX() + 2);
    r.setTop(bounds.getY());
    r.setBottom(center.getY());

    p.addRectangle(r);

    jassert(rotaryStartAngle < rotaryEndAngle); 

    auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));


    g.fillPath(p);

}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

    g.setColour(Colours::red);
    g.drawRect(getLocalBounds());
    g.setColour(Colours::yellow);
    g.drawRect(sliderBounds);

    getLookAndFeel().drawRotarySlider(g, 
        sliderBounds.getX(), 
        sliderBounds.getY(), 
        sliderBounds.getWidth(), 
        sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng, 
        endAng,
        *this);

}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    // make bounds circular

    auto bounds = getLocalBounds();
    
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    size -= getTextHeight() * 2;

    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);


    return r;
}

//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) : audioProcessor(p)
{
    // add listeners to all parameters
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }

    startTimer(60);

}

ResponseCurveComponent::~ResponseCurveComponent()
{
    // if we restister listeners we need to also deregister
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->removeListener(this);
    }
}


void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        // update monoChain

        // grab the mono chain
        auto chainSettings = getChainSettings(audioProcessor.apvts);

        // make coeff
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());


        // update monoChain
        updateCoefficients(monoChain.get<ChainPossitions::Peak>().coefficients, peakCoefficients);
        updateCutFilter(monoChain.get<ChainPossitions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPossitions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

        // signal a repaint
        repaint();

    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    auto responseArea = getLocalBounds();

    auto w = responseArea.getWidth();

    // we want to draw the filter response curve
    // we are going to use the getMagnitudeForFrequency() function
    // that returns the magnitude frequency response of the filter 
    // for a given frequency and sample rate.
    // we need the chain elements first
    auto& lowcut = monoChain.get<ChainPossitions::LowCut>();
    auto& peak = monoChain.get<ChainPossitions::Peak>();
    auto& highcut = monoChain.get<ChainPossitions::HighCut>();
    // we get the sr from the processor
    auto sampleRate = audioProcessor.getSampleRate();
    // a place to store all these magnitudes
    std::vector<double> mags;
    // preallocate the space we need
    mags.resize(w);

    //  iterate through the pixels and draw the filter response
    for (int i = 0; i < w; ++i)
    {

        double mag = 1.f;
        // map our width pixels to log scale 
        // so we can display frequencies correctlly
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

        // get the response curve for each filter on each pixel/freq
        if (!monoChain.isBypassed<ChainPossitions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!lowcut.isBypassed<0>())
            mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<1>())
            mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<2>())
            mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<3>())
            mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!highcut.isBypassed<0>())
            mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<1>())
            mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<2>())
            mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<3>())
            mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        // store the value to our vector in dB
        mags[i] = Decibels::gainToDecibels(mag);

    }

    // Draw response

    Path responseCurve;
    // get min and max of the window
    const double outputMin = responseArea.getBottom();
    const double outpuMax = responseArea.getY();
    // lambda function to help us map the mag value 
    // to our response are height
    auto map = [outputMin, outpuMax](double input)
    {
        return jmap(input, -24.0, +24.0, outputMin, outpuMax);
    };

    // start a new subpath with the first mangitude
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));

    // create lineTo for all the magnitudes
    for (size_t i = 1; i < mags.size(); ++i)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }

    // draw border and path

    g.setColour(Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
}



//==============================================================================
SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor(SimpleEQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    peakFreqSlider    (*audioProcessor.apvts.getParameter("Peak Freq"),      "Hz"),
    peakGainSlider    (*audioProcessor.apvts.getParameter("Peak Gain"),      "dB"),
    peakQSlider       (*audioProcessor.apvts.getParameter("Peak Q"),         ""),
    lowCutFreqSlider  (*audioProcessor.apvts.getParameter("LowCut Freq"),    "Hz"),
    highCutFreqSlider (*audioProcessor.apvts.getParameter("HighCut Freq"),   "Hz"),
    lowCutSlopeSlider (*audioProcessor.apvts.getParameter("LowCut Slope"),   "db/Oct"),
    highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope" ), "db/Oct"),

    responseCurveComponent(audioProcessor),

    peakFreqSliderAttachment    (audioProcessor.apvts, "Peak Freq",     peakFreqSlider),
    peakGainSliderAttachment    (audioProcessor.apvts, "Peak Gain",     peakGainSlider),
    peakQSliderAttachment       (audioProcessor.apvts, "Peak Q",        peakQSlider),
    lowCutFreqSliderAttachment  (audioProcessor.apvts, "LowCut Freq",   lowCutFreqSlider),
    highCutFreqSliderAttachment (audioProcessor.apvts, "HighCut Freq",  highCutFreqSlider),
    lowCutSlopeSliderAttachment (audioProcessor.apvts, "LowCut Slope",  lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)

{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }

    setSize (600, 400);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);
    
}

void SimpleEQAudioProcessorEditor::resized()
{
   
    auto bounds = getLocalBounds();
    // bounds changes size and position after each removeFrom.. call
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

    responseCurveComponent.setBounds(responseArea);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);


    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);


    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQSlider.setBounds(bounds);
   

}



std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQSlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}
