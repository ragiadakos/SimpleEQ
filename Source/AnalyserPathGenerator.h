/*
  ==============================================================================

    AnalyserPathGenerator.h
    Created: 10 Jun 2021 3:46:00pm
    Author:  User

  ==============================================================================
*/

#pragma once
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
