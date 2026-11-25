#include "PluginEditor.h"

namespace
{
    const juce::Colour bgTop    { 0xff0a0a0f };
    const juce::Colour bgBottom { 0xff1a1a2e };
    const juce::Colour accent   { 0xff00ff9d };
    const juce::Colour accent2  { 0xff00d4ff };
    const juce::Colour warn     { 0xffff3366 };
    const juce::Colour amber    { 0xffffaa00 };
}

AudioLimiterEditor::AudioLimiterEditor (AudioLimiterProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setupKnob (threshold, "threshold", "THRESHOLD", " dB");
    setupKnob (ceiling,   "ceiling",   "CEILING",   " dB");
    setupKnob (release,   "release",   "RELEASE",   " ms");
    setupKnob (makeup,    "makeup",    "MAKEUP",    " dB");

    bypass.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.7f));
    bypass.setColour (juce::ToggleButton::tickColourId, warn);
    bypass.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypass);

    setSize (560, 420);
    startTimerHz (30);
}

AudioLimiterEditor::~AudioLimiterEditor()
{
    stopTimer();
}

void AudioLimiterEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, const juce::String& suffix)
{
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.1f));
    k.slider.setColour (juce::Slider::thumbColourId, accent2);
    k.slider.setColour (juce::Slider::textBoxTextColourId, accent);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, accent2);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, id, k.slider);
}

void AudioLimiterEditor::timerCallback()
{
    const float gr = processor.gainReductionDb.load();
    const float pk = processor.outputPeakDb.load();

    // Fast attack, slower fall for readable meters.
    displayGr     = gr > displayGr ? gr : displayGr * 0.88f + gr * 0.12f;
    displayPeakDb = pk > displayPeakDb ? pk : displayPeakDb - 1.2f;

    repaint (meterArea);
}

void AudioLimiterEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (accent);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("LIMITER", getLocalBounds().removeFromTop (60).reduced (20, 8),
                juce::Justification::centredLeft);

    g.setColour (accent.withAlpha (0.5f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("LOOKAHEAD PEAK LIMITER", getLocalBounds().removeFromTop (60).reduced (20, 8),
                juce::Justification::centredRight);

    // Meters: gain reduction (top) and output peak (bottom), drawn as LED segments.
    auto drawMeter = [&] (juce::Rectangle<int> r, float value01, bool reversed, const juce::String& text)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);

        constexpr int segs = 40;
        auto inner = r.reduced (4);
        const float segW = (float) inner.getWidth() / (float) segs;
        const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, value01) * (float) segs);

        for (int i = 0; i < segs; ++i)
        {
            const bool on = i < lit;
            juce::Colour c = accent;
            if (! reversed) c = i > 34 ? warn : (i > 28 ? amber : accent);
            else            c = warn;

            const float x = reversed ? (float) inner.getRight() - (float) (i + 1) * segW
                                     : (float) inner.getX() + (float) i * segW;
            g.setColour (on ? c : juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (x + 0.5f, (float) inner.getY(), segW - 1.0f, (float) inner.getHeight(), 1.5f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (text, r.withTrimmedLeft (6), juce::Justification::centredLeft);
    };

    auto m = meterArea;
    auto grRow = m.removeFromTop (m.getHeight() / 2 - 4);
    m.removeFromTop (8);

    drawMeter (grRow, displayGr / 24.0f, true, "GR  -" + juce::String (displayGr, 1) + " dB");
    drawMeter (m, (displayPeakDb + 40.0f) / 40.0f, false,
               "OUT " + (displayPeakDb <= -99.0f ? juce::String ("-inf") : juce::String (displayPeakDb, 1)) + " dB");
}

void AudioLimiterEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (50);

    auto bottom = area.removeFromBottom (110);
    meterArea = bottom.removeFromTop (70);
    bypass.setBounds (bottom.removeFromTop (34).withSizeKeepingCentre (110, 28));

    auto row = area;
    const int w = row.getWidth() / 4;
    for (Knob* k : { &threshold, &ceiling, &release, &makeup })
    {
        auto cell = row.removeFromLeft (w).reduced (6);
        k->label.setBounds (cell.removeFromTop (20));
        k->slider.setBounds (cell);
    }
}
