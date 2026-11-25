#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class AudioLimiterEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit AudioLimiterEditor (AudioLimiterProcessor&);
    ~AudioLimiterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& suffix);

    AudioLimiterProcessor& processor;

    Knob threshold, ceiling, release, makeup;
    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    float displayGr = 0.0f;      // smoothed gain reduction (dB, positive)
    float displayPeakDb = -100.0f;

    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLimiterEditor)
};
