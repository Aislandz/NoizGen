#pragma once

#include <JuceHeader.h>

//==============================================================================
class WavetableOscillator
{
public:
    WavetableOscillator (const juce::AudioSampleBuffer& wavetableToUse)
        : wavetable (wavetableToUse)
    {
        jassert (wavetable.getNumChannels() == 1);
    }

    void setFrequency (float frequency, float sampleRate)
    {
        auto tableSizeOverSampleRate = (float) wavetable.getNumSamples() / sampleRate;
        tableDelta = frequency * tableSizeOverSampleRate;
    }

    forcedinline float getNextSample() noexcept
    {
        auto tableSize = (unsigned int) wavetable.getNumSamples();

        auto index0 = (unsigned int) currentIndex;              // [6]
        auto index1 = index0 == (tableSize - 1) ? (unsigned int) 0 : index0 + 1;

        auto frac = currentIndex - (float) index0;              // [7]

        auto* table = wavetable.getReadPointer (0);             // [8]
        auto value0 = table[index0];
        auto value1 = table[index1];

        auto currentSample = value0 + frac * (value1 - value0); // [9]

        if ((currentIndex += tableDelta) > (float) tableSize)   // [10]
            currentIndex -= (float) tableSize;

        return currentSample;
    }

private:
    const juce::AudioSampleBuffer& wavetable;
    float currentIndex = 0.0f, tableDelta = 0.0f;
};

//==============================================================================
class MainContentComponent   : public juce::AudioAppComponent,
                               public juce::Timer
{
public:
    MainContentComponent()
    {
        cpuUsageLabel.setText ("CPU Usage", juce::dontSendNotification);
        cpuUsageText.setJustificationType (juce::Justification::right);
        addAndMakeVisible (cpuUsageLabel);
        addAndMakeVisible (cpuUsageText);

        createWavetable();

        setSize (400, 200);
        setAudioChannels (0, 2); // no inputs, two outputs
        startTimer (50);
    }

    ~MainContentComponent() override
    {
        shutdownAudio();
    }

    void resized() override
    {
        cpuUsageLabel.setBounds (10, 10, getWidth() - 20, 20);
        cpuUsageText .setBounds (10, 10, getWidth() - 20, 20);
    }

    void timerCallback() override
    {
        auto cpu = deviceManager.getCpuUsage() * 100;
        cpuUsageText.setText (juce::String (cpu, 6) + " %", juce::dontSendNotification);
    }

    void createWavetable()
    {
        sineTable.setSize (1, (int) tableSize);
        auto* samples = sineTable.getWritePointer (0);                                   // [3]

        auto angleDelta = juce::MathConstants<double>::twoPi / (double) (tableSize - 1); // [4]
        auto currentAngle = 0.0;

        for (unsigned int i = 0; i < tableSize; ++i)
        {
            auto sample = std::sin (currentAngle);                                       // [5]
            samples[i] = (float) sample;
            currentAngle += angleDelta;
        }
    }

    void prepareToPlay (int, double sampleRate) override
    {
        auto numberOfOscillators = 200;

        for (auto i = 0; i < numberOfOscillators; ++i)
        {
            auto* oscillator = new WavetableOscillator (sineTable);

            auto midiNote = juce::Random::getSystemRandom().nextDouble() * 36.0 + 48.0;
            auto frequency = 440.0 * pow (2.0, (midiNote - 69.0) / 12.0);

            oscillator->setFrequency ((float) frequency, (float) sampleRate);
            oscillators.add (oscillator);
        }

        level = 0.25f / (float) numberOfOscillators;
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* leftBuffer  = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
        auto* rightBuffer = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);

        bufferToFill.clearActiveBufferRegion();

        for (auto oscillatorIndex = 0; oscillatorIndex < oscillators.size(); ++oscillatorIndex)
        {
            auto* oscillator = oscillators.getUnchecked (oscillatorIndex);

            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto levelSample = oscillator->getNextSample() * level;

                leftBuffer[sample]  += levelSample;
                rightBuffer[sample] += levelSample;
            }
        }
    }

private:
    juce::Label cpuUsageLabel;
    juce::Label cpuUsageText;

    const unsigned int tableSize = 1 << 7;      // [2]
    float level = 0.0f;

    juce::AudioSampleBuffer sineTable;          // [1]
    juce::OwnedArray<WavetableOscillator> oscillators;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
