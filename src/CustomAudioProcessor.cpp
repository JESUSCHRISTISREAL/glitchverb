#include "CustomAudioProcessor.h"
#include <json/json.hpp>
#include "ui-config.h"

#ifdef RNBO_INCLUDE_DESCRIPTION_FILE
#include <rnbo_description.h>
#endif

//create an instance of our custom plugin, optionally set description, presets and binary data (datarefs)
CustomAudioProcessor* CustomAudioProcessor::CreateDefault() {
	nlohmann::json patcher_desc, presets;

#ifdef RNBO_BINARY_DATA_STORAGE_NAME
	extern RNBO::BinaryDataImpl::Storage RNBO_BINARY_DATA_STORAGE_NAME;
	RNBO::BinaryDataImpl::Storage dataStorage = RNBO_BINARY_DATA_STORAGE_NAME;
#else
	RNBO::BinaryDataImpl::Storage dataStorage;
#endif
	RNBO::BinaryDataImpl data(dataStorage);

#ifdef RNBO_INCLUDE_DESCRIPTION_FILE
	patcher_desc = RNBO::patcher_description;
	presets = RNBO::patcher_presets;
#endif
  return new CustomAudioProcessor(patcher_desc, presets, data);
}

CustomAudioProcessor::CustomAudioProcessor(
    const nlohmann::json& patcher_desc,
    const nlohmann::json& presets,
    const RNBO::BinaryData& data
    ) 
  : RNBO::JuceAudioProcessor(patcher_desc, presets, data) 
{
    // start on the BOUNCYVERB preset. loaded through the state call so it applies right away,
    // and a session the host restores afterwards still wins
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].value("name", "").rfind("BOUNCYVERB", 0) == 0)
        {
            const auto state = presets[i]["preset"].dump();
            setStateInformation(state.data(), (int) state.size());
            _program = (int) i + 1;
            break;
        }
    }
}

void CustomAudioProcessor::setCurrentProgram(int index)
{
    RNBO::JuceAudioProcessor::setCurrentProgram(index);
    _program = index;
}

juce::AudioProcessorEditor* CustomAudioProcessor::createEditor()
{
#if defined(RNBO_EDITOR_NATIVE)
    return new CustomAudioEditor (this, this->_rnboObject);
#elif defined(RNBO_EDITOR_WEBVIEW)
    return new WebBrowserAudioEditor (this, this->_rnboObject);
#else
    return RNBO::JuceAudioProcessor::createEditor();
#endif
}


void CustomAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    RNBO::JuceAudioProcessor::processBlock(buffer, midi);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numChannels == 0)
        return;

    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, numSamples));

    float prev = _outputPeak.load();
    while (peak > prev && ! _outputPeak.compare_exchange_weak(prev, peak)) {}

    // mono sum for the scope. if the editor isn't reading, the fifo just fills up and we skip
    int start1, size1, start2, size2;
    _scopeFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    auto copy = [&](int dest, int src, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            float v = buffer.getSample(0, src + i);
            if (numChannels > 1)
                v = 0.5f * (v + buffer.getSample(1, src + i));
            _scopeData[(size_t) (dest + i)] = v;
        }
    };
    copy(start1, 0, size1);
    copy(start2, size1, size2);
    _scopeFifo.finishedWrite(size1 + size2);
}

int CustomAudioProcessor::readScope(float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    _scopeFifo.prepareToRead(maxSamples, start1, size1, start2, size2);
    std::copy_n(_scopeData.begin() + start1, size1, dest);
    std::copy_n(_scopeData.begin() + start2, size2, dest + size1);
    _scopeFifo.finishedRead(size1 + size2);
    return size1 + size2;
}