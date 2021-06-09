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
    juce::Slider& slider)
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

    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);

        p.addRoundedRectangle(r, 2.f);

        jassert(rotaryStartAngle < rotaryEndAngle); 

        auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));


        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto stringWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(stringWidth + 4, rswl->getTextBoxHeight() + 2);
        r.setCentre(bounds.getCentre());

        g.setColour(Colours::black);
        g.fillRect(r);

        g.setColour(Colours::white);
        g.drawFittedText(text,
            r.toNearestInt(),
            Justification::centred,
            1);

    }





}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();


    /*g.setColour(Colours::red);
    g.drawRect(getLocalBounds());
    g.setColour(Colours::yellow);
    g.drawRect(sliderBounds);*/

    getLookAndFeel().drawRotarySlider(g, 
        sliderBounds.getX(), 
        sliderBounds.getY(), 
        sliderBounds.getWidth(), 
        sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng, 
        endAng,
        *this);


    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5;

    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);

        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);

        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5 + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());

        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);

    }

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

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    juce::String str;
    bool addK = false;
    
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();

        if (val > 999.f)
        {
            val /= 1000.f;
            addK = true;
        }

        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse; // this shoudn't happen
    }

    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";

        str << suffix;
    }

    return str;

}



//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) :
    audioProcessor(p),
    leftChannelFifo(&audioProcessor.leftChannelFifo)
{
    // add listeners to all parameters
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }

    // we are splitting the spectrum 
    // into 2048 equally sized frequency bins
    // theese bins store the magnitude level for
    // the particular range of frequencies
    // 
    /*
    48000 / 2048 = 23Hz per bin    
    */

    leftChannelFFTDataGenerator.changeOrder(FFTOrder::order8192);
    monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());

   
    updateChain();

    startTimerHz(60);

}

ResponseCurveComponent::~ResponseCurveComponent()
{
    // if we register listeners we need to also deregister
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
    // gonna send it to the fft data generator 

    juce::AudioBuffer<float> tempIncomingBuffer;

    //  while there are buffers to pull
    while (leftChannelFifo->getNumCompleteBuffersAvailiable() > 0)
    {
        // if we can pull this buffer
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            // we are gonna send it to the fft data generator
            auto size = tempIncomingBuffer.getNumSamples();

            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                monoBuffer.getReadPointer(0,size),
                monoBuffer.getNumSamples() - size);

            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                tempIncomingBuffer.getReadPointer(0,0),
                size);


            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);


        }
    }

    const auto fftBounds = getAnalysisArea().toFloat();
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();

    const auto binWidth = audioProcessor.getSampleRate() / (double)fftSize;

    while (leftChannelFFTDataGenerator.getNumAvailiableFFTDataBlocks() > 0)
    {
        std::vector<float> fftData;
        if (leftChannelFFTDataGenerator.getFFTData(fftData));
        {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.f);

        }
    }
    
    /*while there are paths that can be pulled
    pull as many as you can 
    display the most recent*/

    while (pathProducer.getNumPathsAvailiable())
    {
        pathProducer.getPath(leftChannelFFTPath);
    }


    if (parametersChanged.compareAndSetBool(false, true))
    {
        //DBG("callback");
       
        updateChain();
        
        // signal a repaint
        //repaint();
    }

    repaint();
}

void ResponseCurveComponent::updateChain()
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

}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    g.drawImage(background, getLocalBounds().toFloat());
    auto responseArea = getAnalysisArea();

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


    leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));


    // draw analyser path
    //g.setColour(juce::Colours::blue);

    float x1 = 0;
    float y1 = getLocalBounds().getCentreY();
    float x2 = getLocalBounds().getRight();
    float y2 = y1;

    Colour red = Colours::red;
    Colour yellow = Colours::yellow;
    Colour green = Colours::limegreen;
    Colour cyan = Colours::cyan;
    Colour blue = Colours::blue;
    Colour purple = Colours::purple;

    ColourGradient grad;

    grad = { red, x1, y1, purple, x2, y2, true };
    grad.addColour(0.5,yellow);
    grad.addColour(0.6,green);
    grad.addColour(0.85,cyan);
    grad.addColour(0.95,blue);

    g.setGradientFill(grad);

    //g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));

    g.fillPath(leftChannelFFTPath);
    

    // draw border and path
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
}

void ResponseCurveComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);

    Graphics g(background);

    Array<float> freqs
    {
        20/*,30,40*/,50,100,
        200/*,300,400*/,500,1000,
        2000/*,3000,4000*/,5000,10000,
        20000
    };

    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();


    Array<float> xs;
    for (auto f : freqs)
    {
        auto normX = mapFromLog10(f, 20.f, 20000.f);
        xs.add(left + width * normX);
    }


    g.setColour(Colours::dimgrey);
    for (auto x : xs)
    {

        g.drawVerticalLine(x,top, bottom);
    }
    
    Array<float> gain
    {
        -24, -12, 0, 12, 24
    };

    for (auto gDb : gain)
    {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));
        g.setColour(gDb == 0.f ? Colour(0u,172u,1u) : Colours::dimgrey);
        g.drawHorizontalLine(y, left, right);
    }

    //g.drawRect(getAnalysisArea());

    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);

    for (int i = 0; i < freqs.size(); ++i)
    {
        auto f = freqs[i];
        auto x = xs[i];

        bool addK = false;
        String str;
        if (f > 999.f)
        {
            addK = true;
            f /= 1000.0f;
        }

        str << f;
        if (addK)
            str << "k";
        str << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(1);

        g.drawFittedText(str, r, Justification::centred, 1);
    }

    for (auto gDb : gain)
    {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));

        String str;
        if (gDb > 0)
            str << "+";
        str << gDb;

        auto textWidth = g.getCurrentFont().getStringWidth(str);


        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(),y);

        g.setColour(gDb == 0.f ? Colour(0u, 172u, 1u) : Colours::lightgrey);

        g.drawFittedText(str, r, Justification::centred, 1);
        
        str.clear();
        str << (gDb - 24.f);

        r.setX(1);
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, Justification::centred, 1);
    }
}


juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();

    //bounds.reduce(10, 8);
        
        //JUCE_LIVE_CONSTANT(5),
        //JUCE_LIVE_CONSTANT(5));

    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);

    return bounds;
}
juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();

    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    
    return bounds;
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
    
    peakFreqSlider.labels.add({ 0.f, "20Hz" });
    peakFreqSlider.labels.add({ 1.f, "20kHz" });
    peakGainSlider.labels.add({ 0.f, "-24dB" });
    peakGainSlider.labels.add({ 1.f, "+24dB" });
    peakQSlider.labels.add({ 0.f, "0.1" });
    peakQSlider.labels.add({ 1.f, "10.0" });
    lowCutFreqSlider.labels.add({ 0.f, "20Hz" });
    lowCutFreqSlider.labels.add({ 1.f, "20kHz" });
    lowCutSlopeSlider.labels.add({ 0.f, "12" });
    lowCutSlopeSlider.labels.add({ 1.f, "48" });
    highCutFreqSlider.labels.add({ 0.f, "20Hz" });
    highCutFreqSlider.labels.add({ 1.f, "20kHz" });
    highCutSlopeSlider.labels.add({ 0.f, "12" });
    highCutSlopeSlider.labels.add({ 1.f, "48" });


    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }


    setSize (600, 480);
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
    // with juce_live_constant you can adjust 
    // this value and see the changes on runtime
    float hRatio = 0.33f;// JUCE_LIVE_CONSTANT(33) / 100.f;

    // bounds changes size and position after each removeFrom.. call
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);

    responseCurveComponent.setBounds(responseArea);
    
    bounds.removeFromTop(5);
    
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
