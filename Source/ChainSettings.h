/*
  ==============================================================================

    ChainSettings.h
    Created: 10 Jun 2021 3:30:53pm
    Author:  User

  ==============================================================================
*/

#pragma once
// struct that contains the filter parameters
struct ChainSettings
{
    float peakFreq{ 0 }, peakGainInDecibells{ 0 }, peakQ{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    
    // Instead of integers the slopes are now expressed in Slope type objects
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };

    bool lowCutBypassed{ false }, peakBypassed{ false }, highCutBypassed{ false }/*, analyserEnabled{ true }*/;

};