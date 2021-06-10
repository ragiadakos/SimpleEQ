/*
  ==============================================================================

    FFTDataGenerator.h
    Created: 10 Jun 2021 3:36:05pm
    Author:  User

  ==============================================================================
*/

#pragma once
#include "FFTOrder.h"

template<typename BlockType>
struct FFTDataGenerator
{
    //produces the FFT data from an audio buffer

    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();
        
        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());

        //first apply a windowing functionto our data
        window->multiplyWithWindowingTable(fftData.data(), fftSize);

        // then render our FFT data..
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

        int numBins = (int)fftSize / 2;

        //normalize the fft values.
        for (int i = 0; i < numBins; ++i)
        {
            fftData[i] /= (float) numBins;
        }

        // convert them to decibels
        for (int i = 0; i < numBins; ++i)
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
        }
    fftDataFifo.push(fftData);
    }   


    void changeOrder(FFTOrder newOrder)
    {
        // when you change order, recreate the window, forwardFFT, fifo, fftData
        // also reset the fifoIndex
         // things that need recreating should be created on the heap via std::make_unique<>

        order = newOrder;
        auto fftSize = getFFTSize();

        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

        fftData.clear();
        fftData.resize(fftSize * 2, 0);

        fftDataFifo.prepare(fftData.size());
    }


    //============================================================
    int getFFTSize() const { return 1 << order; }
    int getNumAvailiableFFTDataBlocks() const { return fftDataFifo.getNumAvailiableForReading(); }
    //============================================================
    bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    Fifo<BlockType> fftDataFifo;
};
