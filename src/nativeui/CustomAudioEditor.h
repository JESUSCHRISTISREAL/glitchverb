#pragma once

#include "JuceHeader.h"
#include "RNBO.h"
#include "CustomAudioProcessor.h"
#include "CustomLookAndFeel.h"
#include "PanelLayout.h"
#include "PresetEncoder.h"

class CustomAudioEditor : public AudioProcessorEditor, private Timer
{
public:
    CustomAudioEditor (CustomAudioProcessor* const p, RNBO::CoreObject& rnboObject);
    ~CustomAudioEditor() override;
    void paint (Graphics& g) override;
    void resized() override;

    // for rendering the faceplate: 0 = panel print, 1 = scope graticule (design units)
    void paintArtwork (Graphics& g, int layer);
    Rectangle<float> getAccentBar() const;

    // reads new scope/meter data, normally called by the timer
    void refresh();

private:
    void timerCallback() override { refresh(); }
    void setupKnob (Slider& s, const PanelLayout::Knob& k);
    void renderBackground();
    void renderLcdBase();
    void drawPrint (Graphics& g);
    void drawKnobPrint (Graphics& g, const PanelLayout::Knob& k, const String& name, const String& minText, const String& maxText);
    void drawLcd (Graphics& g);
    void rasterize();
    void lcdText (int x, int y, const String& text);
    Rectangle<float> lcdPixel (int column, int row) const;
    void drawPresetDisplay (Graphics& g);
    void drawGrain (Graphics& g);
    Image makeGrain (int width, int height);
    void stepPreset (int direction);
    String clockText (double value) const;
    String tailText() const;

    CustomAudioProcessor* _audioProcessor;
    RNBO::CoreObject&     _rnboObject;

    PanelLookAndFeel _lookAndFeel;

    // sliders before attachments, the attachments register with them on construction
    Slider _clockSpeedSlider, _decaySlider, _dryWetSlider;
    PresetEncoder _presetEncoder;

    SliderParameterAttachment _clockSpeedAttachment;
    SliderParameterAttachment _decayAttachment;
    SliderParameterAttachment _dryWetAttachment;

    StringArray _presetNames;
    int _shownProgram = -1;

    Image _faceplate, _glass, _meterLit, _background, _grain;
    float _scale = 1.0f;

    // last stretch of output for the scope
    static constexpr int scopeLength = 1024;
    std::vector<float> _scope = std::vector<float> (scopeLength, 0.0f);
    std::vector<float> _readBuffer = std::vector<float> (8192);
    float _scopeRange = 1.0f;
    int _rangeHold = 0, _quietFrames = 0;
    float _meterDb = -100.0f;

    // the screen is a 128 x 72 dot matrix lcd
    static constexpr int lcdColumns = 128, lcdRows = 72;
    std::array<uint8_t, lcdColumns * lcdRows> _lit {}, _previousLit {};
    Image _lcdBase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioEditor)
};