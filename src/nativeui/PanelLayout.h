#pragma once

#include "JuceHeader.h"

// Panel geometry in design units. The window scales all of it, and the
// faceplate render uses the same numbers, so change both together.
namespace PanelLayout
{
    constexpr float width  = 760.0f;
    constexpr float height = 480.0f;

    const Rectangle<float> bezel     { 20.0f, 72.0f, 380.0f, 232.0f };
    const Rectangle<float> screen    { 36.0f, 88.0f, 348.0f, 200.0f };
    const Rectangle<float> presetBox { 20.0f, 316.0f, 380.0f, 104.0f };
    const Rectangle<float> reverbBox { 412.0f, 72.0f, 328.0f, 348.0f };
    const Rectangle<float> meter     { 472.0f, 449.0f, 264.0f, 14.0f };
    // the grey header and footer bands, inside the seams the faceplate render cuts round them
    const Rectangle<float> headerBand { 0.0f, 0.0f, 760.0f, 55.0f };
    const Rectangle<float> footerBand { 0.0f, 433.0f, 760.0f, 47.0f };
    const Rectangle<float> presetDisplay { 100.0f, 361.0f, 294.0f, 26.0f };

    struct Knob
    {
        Rectangle<float> bounds;
        float textBoxHeight;
        float textBoxWidth;
    };

    // the three knobs are the same size; the gaps above, between and below them in the reverb box are equal
    const Knob clock  { { 501.0f,  97.0f, 150.0f, 144.0f }, 24.0f, 130.0f };
    const Knob decay  { { 425.0f, 266.0f, 150.0f, 144.0f }, 24.0f, 130.0f };
    const Knob dryWet { { 577.0f, 266.0f, 150.0f, 144.0f }, 24.0f, 130.0f };
    const Knob encoder { {  28.0f, 342.0f,  64.0f,  64.0f },  0.0f,  0.0f };

    // dot pitch of the readouts and the preset window, a little finer than the LCD's 2.7
    constexpr float dotPitch = 2.5f;

    inline Rectangle<float> dial (const Knob& k)   { return k.bounds.withTrimmedBottom (k.textBoxHeight); }
    inline float dialRadius (const Knob& k)         { auto d = dial (k); return jmin (d.getWidth(), d.getHeight()) * 0.5f - 4.0f; }
    inline Rectangle<float> readout (const Knob& k)
    {
        return { k.bounds.getCentreX() - k.textBoxWidth * 0.5f, k.bounds.getBottom() - k.textBoxHeight, k.textBoxWidth, k.textBoxHeight };
    }
}