#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout AudioLimiterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "threshold", 1 }, "Threshold",
        NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -6.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "ceiling", 1 }, "Ceiling",
        NormalisableRange<float> (-3.0f, 0.0f, 0.1f), -0.3f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "makeup", 1 }, "Makeup Gain",
        NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

AudioLimiterProcessor::AudioLimiterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    thresholdParam = apvts.getRawParameterValue ("threshold");
    ceilingParam   = apvts.getRawParameterValue ("ceiling");
    releaseParam   = apvts.getRawParameterValue ("release");
    makeupParam    = apvts.getRawParameterValue ("makeup");
    bypassParam    = apvts.getRawParameterValue ("bypass");
}

bool AudioLimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

void AudioLimiterProcessor::prepareToPlay (double sampleRate, int)
{
    preparedChannels = juce::jmax (1, getTotalNumOutputChannels());
    core.prepare (sampleRate, preparedChannels, 5.0);
    // The lookahead delays audio; tell the host so it can compensate.
    setLatencySamples (core.getLatencySamples());
}

void AudioLimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numChannels = juce::jmin (buffer.getNumChannels(), preparedChannels);
    const int numSamples = buffer.getNumSamples();

    const float thresholdDb = thresholdParam->load();
    const float ceilingDb   = ceilingParam->load();
    const float makeupDb    = makeupParam->load();

    // Limit to the threshold, but never so high that threshold + makeup would
    // pass the ceiling. This way the ceiling is reached by limiting, not by clipping.
    const float limitDb = juce::jmin (thresholdDb, ceilingDb - makeupDb);

    core.setReleaseMs (releaseParam->load());

    const float minGain = core.process (buffer.getArrayOfWritePointers(), numChannels, numSamples,
                                        juce::Decibels::decibelsToGain (limitDb),
                                        juce::Decibels::decibelsToGain (makeupDb),
                                        juce::Decibels::decibelsToGain (ceilingDb),
                                        bypassParam->load() > 0.5f);

    const bool bypassed = bypassParam->load() > 0.5f;
    gainReductionDb.store (bypassed ? 0.0f : -juce::Decibels::gainToDecibels (minGain, -60.0f));

    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
    outputPeakDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
}

juce::AudioProcessorEditor* AudioLimiterProcessor::createEditor()
{
    return new AudioLimiterEditor (*this);
}

void AudioLimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioLimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioLimiterProcessor();
}
