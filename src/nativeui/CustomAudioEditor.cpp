#include "CustomAudioEditor.h"
#include "UIColours.h"
#include "UIFonts.h"
#include "BinaryData.h"
#include "LcdFont.h"

namespace
{
    constexpr float knobStart = MathConstants<float>::pi * 1.25f;
    constexpr float knobEnd   = MathConstants<float>::pi * 2.75f;

    Point<float> pointAt (Point<float> c, float radius, float angle)
    {
        return { c.x + radius * std::sin (angle), c.y - radius * std::cos (angle) };
    }

    float textWidth (const Font& font, const String& text)
    {
        GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    }

    Image loadImage (const void* data, int size)
    {
        return ImageCache::getFromMemory (data, size);
    }

    // the glass render is a dark sheet with a faint sheen on it. laid over the lcd it greys the
    // backlight, so keep just the sheen, as white that only brightens
    Image glassSheen (const Image& glass)
    {
        if (! glass.isValid())
            return {};

        const Image::BitmapData in (glass, Image::BitmapData::readOnly);
        float low = 1.0f, high = 0.0f;
        for (int y = 0; y < glass.getHeight(); ++y)
            for (int x = 0; x < glass.getWidth(); ++x)
            {
                const float v = in.getPixelColour (x, y).getPerceivedBrightness();
                low = jmin (low, v);
                high = jmax (high, v);
            }

        Image sheen (Image::ARGB, glass.getWidth(), glass.getHeight(), true);
        Image::BitmapData out (sheen, Image::BitmapData::writeOnly);
        const float range = jmax (0.001f, high - low);
        for (int y = 0; y < glass.getHeight(); ++y)
            for (int x = 0; x < glass.getWidth(); ++x)
            {
                const float v = (in.getPixelColour (x, y).getPerceivedBrightness() - low) / range;
                out.setPixelColour (x, y, Colours::white.withAlpha (0.16f * v));
            }
        return sheen;
    }

    // measured RT60 in seconds, rows decay 0 to 1, columns clock 0 to 1, both in steps of 0.1.
    // a 50 ms noise burst through the patch at 48 kHz (44.1 kHz is within a few percent).
    // anything past 40 s was extended along the curve the shorter tails follow
    const float tailSeconds[11][11] =
    {
        { 141.0f, 127.0f,  77.0f, 32.59f, 16.42f,  8.80f,  5.23f,  3.24f,  2.20f,  1.53f,  1.05f },
        { 136.0f, 123.0f,  75.0f, 33.66f, 17.09f,  9.17f,  5.47f,  3.40f,  2.32f,  1.61f,  1.13f },
        { 141.0f, 128.0f,  79.0f, 35.86f, 18.41f,  9.92f,  5.93f,  3.71f,  2.53f,  1.76f,  1.25f },
        { 156.0f, 142.0f,  87.0f, 42.00f, 20.39f, 11.03f,  6.60f,  4.14f,  2.82f,  1.97f,  1.40f },
        { 175.0f, 160.0f,  99.0f, 48.00f, 23.95f, 12.98f,  7.77f,  4.90f,  3.34f,  2.34f,  1.69f },
        { 216.0f, 197.0f, 122.0f, 59.00f, 29.56f, 16.04f,  9.62f,  6.07f,  4.11f,  2.91f,  2.09f },
        { 275.0f, 250.0f, 155.0f, 76.00f, 37.04f, 20.74f, 12.40f,  7.84f,  5.30f,  3.77f,  2.72f },
        { 384.0f, 350.0f, 216.0f, 106.0f, 53.00f, 28.49f, 17.04f, 10.73f,  7.27f,  5.18f,  3.72f },
        { 607.0f, 553.0f, 340.0f, 165.0f, 82.00f, 44.00f, 26.15f, 16.37f, 11.15f,  7.94f,  5.65f },
        { 999.0f, 999.0f, 753.0f, 358.0f, 174.0f, 93.00f, 54.00f, 32.67f, 22.42f, 15.92f, 11.20f },
        { 999.0f, 999.0f, 999.0f, 603.0f, 304.0f, 167.0f, 100.0f, 64.00f, 43.00f, 31.71f, 22.41f },
    };
}

static juce::RangedAudioParameter&
findParameter(RNBO::JuceAudioProcessor* p, const juce::String& name)
{
    for (auto* param : p->getParameters())
    {
        if (param->getName(128) == name)
            return static_cast<juce::RangedAudioParameter&>(*param);
    }

    throw std::runtime_error("Parameter not found: " + name.toStdString());
}

CustomAudioEditor::CustomAudioEditor (CustomAudioProcessor* const p,
                                      RNBO::CoreObject& rnboObject)
    : AudioProcessorEditor (p)
    , _audioProcessor (p)
    , _rnboObject (rnboObject)
    , _clockSpeedAttachment(findParameter(p, "clock_speed"), _clockSpeedSlider)
    , _decayAttachment(findParameter(p, "decay"), _decaySlider)
    , _dryWetAttachment(findParameter(p, "dry_wet"), _dryWetSlider)
{
    _faceplate = loadImage (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
    _glass     = glassSheen (loadImage (BinaryData::glass_png, BinaryData::glass_pngSize));
    _meterLit  = loadImage (BinaryData::meter_lit_png, BinaryData::meter_lit_pngSize);
    _lookAndFeel.setKnobStrip (loadImage (BinaryData::knobs_png, BinaryData::knobs_pngSize), 128);
    _lookAndFeel.setEncoderStrip (loadImage (BinaryData::encoder_png, BinaryData::encoder_pngSize), 96);
    setLookAndFeel (&_lookAndFeel);

    setupKnob (_clockSpeedSlider, PanelLayout::clock);
    setupKnob (_decaySlider, PanelLayout::decay);
    setupKnob (_dryWetSlider, PanelLayout::dryWet);

    auto percent = [] (double v) { return String (roundToInt (v * 100.0)) + " %"; };
    _clockSpeedSlider.textFromValueFunction = [this] (double v) { return clockText (v); };
    _decaySlider.textFromValueFunction = percent;
    _dryWetSlider.textFromValueFunction = percent;

    const std::pair<Slider*, const char*> knobs[] = { { &_clockSpeedSlider, "clock_speed" },
                                                      { &_decaySlider, "decay" },
                                                      { &_dryWetSlider, "dry_wet" } };
    for (auto& [slider, id] : knobs)
    {
        auto& param = findParameter (p, id);
        slider->setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        slider->updateText();
    }

    // program 0 is the patch's starting state, the presets are 1 onwards
    for (int i = 1; i < p->getNumPrograms(); ++i)
        _presetNames.add (p->getProgramName (i));

    _presetEncoder.onStep = [this] (int direction) { stepPreset (direction); };
    addAndMakeVisible (_presetEncoder);

    setResizable (true, true);
    setResizeLimits (570, 360, 1140, 720);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (PanelLayout::width / PanelLayout::height);
    setSize ((int) PanelLayout::width, (int) PanelLayout::height);

    refresh();
    startTimerHz (30);
}

CustomAudioEditor::~CustomAudioEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CustomAudioEditor::setupKnob (Slider& s, const PanelLayout::Knob& k)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (knobStart, knobEnd, true);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, (int) k.textBoxWidth, (int) k.textBoxHeight);
    addAndMakeVisible (s);
}

String CustomAudioEditor::clockText (double value) const
{
    // same curve as the patch: 500 Hz up to the host rate, cubed
    double sr = _audioProcessor->getSampleRate();
    if (sr <= 0.0)
        sr = 48000.0;

    const double hz = 500.0 + (sr - 500.0) * value * value * value;
    if (hz < 1000.0)
        return String (roundToInt (hz)) + " Hz";
    return String (hz / 1000.0, 1) + " kHz";
}

String CustomAudioEditor::tailText() const
{
    // bilinear in log time, since the tail grows roughly exponentially with decay
    const float d = jlimit (0.0f, 1.0f, (float) _decaySlider.getValue()) * 10.0f;
    const float c = jlimit (0.0f, 1.0f, (float) _clockSpeedSlider.getValue()) * 10.0f;
    const int d0 = jmin ((int) d, 9), c0 = jmin ((int) c, 9);
    const float fd = d - (float) d0, fc = c - (float) c0;

    auto at = [] (int row, int col) { return std::log (tailSeconds[row][col]); };
    const float top = at (d0, c0) + (at (d0, c0 + 1) - at (d0, c0)) * fc;
    const float bottom = at (d0 + 1, c0) + (at (d0 + 1, c0 + 1) - at (d0 + 1, c0)) * fc;
    const float seconds = std::exp (top + (bottom - top) * fd);

    if (seconds >= 60.0f)
        return ">60 s";
    if (seconds >= 10.0f)
        return String (roundToInt (seconds)) + " s";
    return String (seconds, 1) + " s";
}

void CustomAudioEditor::resized()
{
    _scale = (float) getWidth() / PanelLayout::width;

    auto place = [this] (Slider& s, const PanelLayout::Knob& k)
    {
        s.setBounds ((k.bounds * _scale).toNearestInt());
        if (k.textBoxHeight > 0.0f)
            s.setTextBoxStyle (Slider::TextBoxBelow, false, roundToInt (k.textBoxWidth * _scale), roundToInt (k.textBoxHeight * _scale));
    };
    place (_clockSpeedSlider, PanelLayout::clock);
    place (_decaySlider, PanelLayout::decay);
    place (_dryWetSlider, PanelLayout::dryWet);
    _presetEncoder.setBounds ((PanelLayout::encoder.bounds * _scale).toNearestInt());

    renderBackground();
    renderLcdBase();
}

void CustomAudioEditor::renderBackground()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // drawn at 2x so it stays sharp on high-dpi screens
    _background = Image (Image::ARGB, getWidth() * 2, getHeight() * 2, true);
    Graphics g (_background);
    g.addTransform (AffineTransform::scale (2.0f * _scale));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    if (_faceplate.isValid())
    {
        g.drawImage (_faceplate, { 0.0f, 0.0f, PanelLayout::width, PanelLayout::height });
        return;
    }

    g.fillAll (UIColours::panel);
    drawPrint (g);
}

void CustomAudioEditor::paintArtwork (Graphics& g, int layer)
{
    // the screen is drawn live, so only the print goes into the render
    if (layer != 0)
        return;

    PanelLookAndFeel::maskOnly = true;
    drawPrint (g);
    PanelLookAndFeel::maskOnly = false;
}

Rectangle<float> CustomAudioEditor::getAccentBar() const
{
    return { 24.0f + textWidth (UIFonts::name (30.0f), "Glitch Verb") + 14.0f, 15.0f, 2.0f, 34.0f };
}

void CustomAudioEditor::drawKnobPrint (Graphics& g, const PanelLayout::Knob& k, const String& name,
                                       const String& minText, const String& maxText)
{
    using P = PanelLookAndFeel;
    const auto c = PanelLayout::dial (k).getCentre();
    const float r = PanelLayout::dialRadius (k);
    const bool big = k.bounds.getWidth() > 100.0f;

    P::print (g, name, { k.bounds.getX() - 20.0f, k.bounds.getY() - 15.0f, k.bounds.getWidth() + 40.0f, 12.0f },
              UIFonts::label (big ? 11.5f : 10.5f), Justification::centred);

    for (int i = 0; i <= 20; ++i)
    {
        const float angle = knobStart + (knobEnd - knobStart) * (float) i / 20.0f;
        const bool major = i % 2 == 0;
        g.setColour (PanelLookAndFeel::maskOnly ? Colours::white.withAlpha (major ? 0.95f : 0.6f)
                                                : UIColours::ink.withAlpha (major ? 0.8f : 0.5f));
        g.drawLine ({ pointAt (c, r * (major ? 0.86f : 0.90f), angle), pointAt (c, r, angle) }, major ? 1.5f : 1.0f);
    }

    const auto font = UIFonts::label (big ? 8.5f : 8.0f);
    P::print (g, minText, Rectangle<float> (34.0f, 10.0f).withCentre (pointAt (c, r + 3.0f, knobStart).translated (-6.0f, 6.0f)),
              font, Justification::centred, 0.75f);
    P::print (g, maxText, Rectangle<float> (34.0f, 10.0f).withCentre (pointAt (c, r + 3.0f, knobEnd).translated (6.0f, 6.0f)),
              font, Justification::centred, 0.75f);
}

void CustomAudioEditor::drawPrint (Graphics& g)
{
    using P = PanelLookAndFeel;
    const float bar = getAccentBar().getX();

    P::print (g, "Glitch Verb", { 24.0f, 8.0f, bar - 24.0f, 44.0f }, UIFonts::name (30.0f), Justification::centredLeft);
    P::print (g, "RECLOCKABLE REVERB", { bar + 14.0f, 21.0f, 400.0f, 18.0f }, UIFonts::label (14.0f), Justification::centredLeft);

    P::print (g, "DSP BY JONATHAN HIGGINS   |   GUI BY SFOG7", { 24.0f, 449.0f, 420.0f, 14.0f },
              UIFonts::label (9.5f), Justification::centredLeft, 0.7f);

    // knobs
    P::print (g, "REVERB", { 424.0f, 78.0f, 100.0f, 11.0f }, UIFonts::label (9.5f), Justification::centredLeft, 0.6f);
    drawKnobPrint (g, PanelLayout::clock, "CLOCK", "500", "HOST");
    drawKnobPrint (g, PanelLayout::decay, "DECAY", "0", "100");
    drawKnobPrint (g, PanelLayout::dryWet, "DRY / WET", "DRY", "WET");

    P::print (g, "PRESET", { 32.0f, 322.0f, 100.0f, 11.0f }, UIFonts::label (9.5f), Justification::centredLeft, 0.6f);
}

void CustomAudioEditor::drawGrain (Graphics& g)
{
    // powdercoat on the grey bands: one faint speck per screen pixel, so it is made at the
    // screen's own size and drawn without scaling
    const float k = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int w = roundToInt ((float) getWidth() * k), h = roundToInt ((float) getHeight() * k);
    if (w <= 0 || h <= 0)
        return;

    if (_grain.getWidth() != w || _grain.getHeight() != h)
        _grain = makeGrain (w, h);

    Graphics::ScopedSaveState state (g);
    g.addTransform (AffineTransform::scale (1.0f / k));
    g.drawImageAt (_grain, 0, 0);
}

Image CustomAudioEditor::makeGrain (int width, int height)
{
    const float s = (float) width / PanelLayout::width;

    // where the grain goes, worked out in whole screen pixels so no edge pixel is half covered:
    // the grey bands, but not under the print, the amber bar or the led wells, which keep a
    // pixel to spare round them
    auto inside = [s] (Rectangle<float> r)
    {
        const int x0 = (int) std::ceil (r.getX() * s), y0 = (int) std::ceil (r.getY() * s);
        const int x1 = (int) std::floor (r.getRight() * s), y1 = (int) std::floor (r.getBottom() * s);
        return Rectangle<int> (x0, y0, jmax (0, x1 - x0), jmax (0, y1 - y0));
    };
    auto around = [s] (Rectangle<float> r) { return (r * s).getSmallestIntegerContainer().expanded (1); };

    Image where (Image::SingleChannel, width, height, true);
    {
        Graphics g (where);
        g.setColour (Colours::white);
        g.fillRect (inside (PanelLayout::headerBand));
        g.fillRect (inside (PanelLayout::footerBand));
    }
    // cleared outright: on a one channel image even black paint marks the pixels
    where.clear (around (getAccentBar().expanded (0.6f)));
    const auto m = PanelLayout::meter;
    for (int i = 0; i < 16; ++i)
        where.clear (around (Rectangle<float> (m.getX() + 4.0f + 16.0f * (float) i, m.getY() + 4.0f, 12.0f, 6.0f).expanded (0.8f)));
    Image print (Image::SingleChannel, width, height, true);
    {
        Graphics g (print);
        g.addTransform (AffineTransform::scale (s));
        paintArtwork (g, 0);
    }

    // the speck pattern, a 128 pixel tile: each pixel a little lighter or darker than the base,
    // and now and then a brighter fleck
    constexpr int tile = 128;
    std::vector<float> speck (tile * tile);
    Random random (7);
    for (auto& v : speck)
    {
        const float u1 = jmax (1.0e-6f, random.nextFloat()), u2 = random.nextFloat();
        const float gauss = std::sqrt (-2.0f * std::log (u1)) * std::cos (MathConstants<float>::twoPi * u2);
        v = gauss > 0.0f ? jmin (gauss * 0.035f, 0.12f) : -jmin (-gauss * 0.05f, 0.16f);
        if (random.nextFloat() < 0.006f)
            v = 0.15f;
    }

    // straight through the pixel rows, since this runs each time the window changes size
    Image grain (Image::ARGB, width, height, true);
    const Image::BitmapData in (where, Image::BitmapData::readOnly);
    const Image::BitmapData ink (print, Image::BitmapData::readOnly);
    Image::BitmapData out (grain, Image::BitmapData::writeOnly);
    const int headerEnd = jmin (height, (int) std::ceil (PanelLayout::headerBand.getBottom() * s));
    const int footerStart = jmax (0, (int) (PanelLayout::footerBand.getY() * s));
    for (int y = 0; y < height; ++y)
    {
        if (y >= headerEnd && y < footerStart)
            continue;
        const uint8* band = in.getLinePointer (y);
        const uint8* printed = ink.getLinePointer (y);
        uint8* dst = out.getLinePointer (y);
        for (int x = 0; x < width; ++x)
        {
            const int cover = band[x * in.pixelStride] * (255 - printed[x * ink.pixelStride]);
            if (cover == 0)
                continue;
            const float v = speck[(size_t) ((y % tile) * tile + x % tile)];
            const auto alpha = (uint8) jlimit (0, 255, roundToInt (std::abs (v) * (float) cover / 255.0f));
            const uint8 level = v > 0.0f ? alpha : 0;   // premultiplied white or black
            reinterpret_cast<PixelARGB*> (dst + x * out.pixelStride)->setARGB (alpha, level, level, level);
        }
    }
    return grain;
}

void CustomAudioEditor::drawPresetDisplay (Graphics& g)
{
    // preset name in the readout window next to the encoder
    String text = "INIT";
    if (_shownProgram > 0 && _shownProgram <= _presetNames.size())
        text = _presetNames[_shownProgram - 1].toUpperCase();

    LcdFont::drawWindow (g, PanelLayout::presetDisplay, text, PanelLayout::dotPitch, UIColours::readout);
}

void CustomAudioEditor::stepPreset (int direction)
{
    const int n = _presetNames.size();
    if (n == 0)
        return;

    // wraps round both ways. from the starting state the first click lands on the first or last preset
    const int current = _audioProcessor->getCurrentProgram();
    int next;
    if (current < 1 || current > n)
        next = direction > 0 ? 0 : n - 1;
    else
        next = ((current - 1 + direction) % n + n) % n;

    _audioProcessor->setCurrentProgram (next + 1);
    refresh();
}

Rectangle<float> CustomAudioEditor::lcdPixel (int column, int row) const
{
    const auto scr = PanelLayout::screen;
    const float pitch = 2.7f;
    const float x0 = scr.getX() + (scr.getWidth() - pitch * (float) lcdColumns) * 0.5f;
    const float y0 = scr.getY() + (scr.getHeight() - pitch * (float) lcdRows) * 0.5f;
    return { x0 + pitch * (float) column + 0.2f, y0 + pitch * (float) row + 0.2f, pitch - 0.4f, pitch - 0.4f };
}

void CustomAudioEditor::renderLcdBase()
{
    // backlight and the faint unlit dots, only redrawn when the window size changes
    const auto scr = PanelLayout::screen;
    _lcdBase = Image (Image::ARGB, jmax (1, roundToInt (scr.getWidth() * _scale * 2.0f)),
                      jmax (1, roundToInt (scr.getHeight() * _scale * 2.0f)), true);

    Graphics g (_lcdBase);
    g.addTransform (AffineTransform::translation (-scr.getX(), -scr.getY()).scaled (2.0f * _scale));
    g.setGradientFill (ColourGradient (UIColours::lcdLight, scr.getCentreX(), scr.getCentreY() - 20.0f,
                                       UIColours::lcdEdge, scr.getX(), scr.getY(), true));
    g.fillRect (scr);

    g.setColour (UIColours::lcdPixel.withAlpha (0.05f));
    for (int row = 0; row < lcdRows; ++row)
        for (int col = 0; col < lcdColumns; ++col)
            g.fillRect (lcdPixel (col, row));
}

void CustomAudioEditor::lcdText (int x, int y, const String& text)
{
    for (auto c : text)
    {
        if (auto* glyph = LcdFont::find ((char) c))
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (glyph->rows[row][col] == '#' && x + col >= 0 && x + col < lcdColumns && y + row >= 0 && y + row < lcdRows)
                        _lit[(size_t) ((y + row) * lcdColumns + x + col)] = 1;
        x += 6;
    }
}

void CustomAudioEditor::rasterize()
{
    _previousLit = _lit;
    _lit.fill (0);

    auto set = [this] (int col, int row)
    {
        if (col >= 0 && col < lcdColumns && row >= 0 && row < lcdRows)
            _lit[(size_t) (row * lcdColumns + col)] = 1;
    };

    constexpr int top = 10, bottom = 62, mid = 36;
    const int textBottom = lcdRows - 8;

    lcdText (1, 1, "CLOCK " + clockText (_clockSpeedSlider.getValue()));

    if (_quietFrames > 15)
    {
        lcdText ((lcdColumns - 53) / 2, mid - 3, "NO SIGNAL");
        return;
    }

    // dotted centre line and divisions
    for (int col = 0; col < lcdColumns; col += 4)
        set (col, mid);
    for (int col = 32; col < lcdColumns; col += 32)
        for (int row = top; row <= bottom; row += 4)
            set (col, row);

    // each column covers a few samples and is drawn from its lowest to its highest point,
    // starting where the last column ended so the line stays joined up
    const int perColumn = scopeLength / lcdColumns;
    const float half = (float) (bottom - top) * 0.5f;
    auto rowFor = [&] (float v) { return jlimit (top, bottom, roundToInt ((float) mid - v / _scopeRange * half)); };

    int lastRow = rowFor (_scope[0]);
    for (int col = 0; col < lcdColumns; ++col)
    {
        int lo = lastRow, hi = lastRow;
        for (int i = col * perColumn; i < (col + 1) * perColumn; ++i)
        {
            lastRow = rowFor (_scope[(size_t) i]);
            lo = jmin (lo, lastRow);
            hi = jmax (hi, lastRow);
        }
        for (int row = lo; row <= hi; ++row)
            set (col, row);
    }

    lcdText (1, textBottom, "TAIL " + tailText());
}

void CustomAudioEditor::drawLcd (Graphics& g)
{
    const auto scr = PanelLayout::screen;
    if (_lcdBase.isValid())
        g.drawImage (_lcdBase, scr);

    // lit pixels, and last frame's still fading out since the crystal is slow
    g.setColour (UIColours::lcdPixel.withAlpha (0.3f));
    for (size_t i = 0; i < _lit.size(); ++i)
        if (_previousLit[i] && ! _lit[i])
            g.fillRect (lcdPixel ((int) i % lcdColumns, (int) i / lcdColumns));

    g.setColour (UIColours::lcdPixel.withAlpha (0.88f));
    for (size_t i = 0; i < _lit.size(); ++i)
        if (_lit[i])
            g.fillRect (lcdPixel ((int) i % lcdColumns, (int) i / lcdColumns));

    if (_glass.isValid())
        g.drawImage (_glass, scr);
}
void CustomAudioEditor::paint (Graphics& g)
{
    if (_background.isValid())
        g.drawImage (_background, getLocalBounds().toFloat());
    drawGrain (g);

    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::scale (_scale));
    drawLcd (g);
    drawPresetDisplay (g);

    // level meter: 16 leds from -45 to 0 dB, cut out of the all-lit render
    if (_meterLit.isValid())
    {
        const auto m = PanelLayout::meter;
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        for (int i = 0; i < 16 && _meterDb >= -45.0f + 3.0f * (float) i; ++i)
        {
            Graphics::ScopedSaveState led (g);
            g.reduceClipRegion (Rectangle<float> (m.getX() + 2.0f + 16.0f * (float) i, m.getY() + 2.0f, 16.0f, 10.0f).toNearestInt());
            g.drawImage (_meterLit, m);
        }
    }
}

void CustomAudioEditor::refresh()
{
    bool gotAudio = false;
    for (int got; (got = _audioProcessor->readScope (_readBuffer.data(), (int) _readBuffer.size())) > 0;)
    {
        gotAudio = true;
        const int fresh = jmin (got, scopeLength);
        const int keep = scopeLength - fresh;
        std::copy (_scope.end() - keep, _scope.end(), _scope.begin());
        std::copy (_readBuffer.begin() + (got - fresh), _readBuffer.begin() + got, _scope.begin() + keep);
    }

    float peak = 0.0f;
    for (auto v : _scope)
        peak = jmax (peak, std::abs (v));

    _quietFrames = (gotAudio && peak > 0.001f) ? 0 : _quietFrames + 1;
    if (! gotAudio && _quietFrames > 15)
        std::fill (_scope.begin(), _scope.end(), 0.0f);

    // vertical range: grow straight away, shrink after about a second
    static constexpr float ranges[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f };
    float target = ranges[4];
    for (auto r : ranges)
        if (peak * 1.1f <= r) { target = r; break; }

    if (target > _scopeRange)                         { _scopeRange = target; _rangeHold = 0; }
    else if (target < _scopeRange && ++_rangeHold > 30) { _scopeRange = target; _rangeHold = 0; }
    else if (target >= _scopeRange)                   _rangeHold = 0;

    rasterize();

    _meterDb = jmax (Decibels::gainToDecibels (_audioProcessor->takeOutputPeak(), -100.0f), _meterDb - 1.6f);

    const int program = _audioProcessor->getCurrentProgram();
    if (program != _shownProgram)
    {
        _shownProgram = program;
        repaint ((PanelLayout::presetDisplay * _scale).toNearestInt().expanded (2));
    }

    repaint ((PanelLayout::screen * _scale).toNearestInt().expanded (2));
    repaint ((PanelLayout::meter * _scale).toNearestInt().expanded (2));
}