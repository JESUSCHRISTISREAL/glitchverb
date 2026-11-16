#pragma once

#include "RNBO.h"
#include "RNBO_Utils.h"
#include "RNBO_JuceAudioProcessor.h"
#include "RNBO_BinaryData.h"
#include <json/json.hpp>

class CustomAudioProcessor : public RNBO::JuceAudioProcessor {
public:
    static CustomAudioProcessor* CreateDefault();
    CustomAudioProcessor(const nlohmann::json& patcher_desc, const nlohmann::json& presets, const RNBO::BinaryData& data);
    juce::AudioProcessorEditor* createEditor() override;

    int getCurrentProgram() override { return _program; }
    void setCurrentProgram(int index) override;

    using RNBO::JuceAudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    // output for the scope and meter, read from the editor's timer
    int readScope(float* dest, int maxSamples);
    float takeOutputPeak() { return _outputPeak.exchange(0.0f); }

private:
    juce::AbstractFifo _scopeFifo { 32768 };
    std::vector<float> _scopeData = std::vector<float>(32768);
    std::atomic<float> _outputPeak { 0.0f };
    int _program = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioProcessor)
};