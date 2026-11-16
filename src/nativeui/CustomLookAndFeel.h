#pragma once

#include "JuceHeader.h"
#include "UIColours.h"
#include "UIFonts.h"
#include "LcdFont.h"
#include "PanelLayout.h"

// Draws the rendered knobs, the lit value arcs and the readout digits.
// Everything else on the panel (grooves, windows, print) is in the faceplate image.
class PanelLookAndFeel : public LookAndFeel_V4
{
public:
    PanelLookAndFeel()
    {
        setColour(Slider::textBoxTextColourId, UIColours::readout);
        setColour(Slider::textBoxBackgroundColourId, UIColours::transparent);
        setColour(Slider::textBoxOutlineColourId, UIColours::transparent);
        setColour(Slider::textBoxHighlightColourId, UIColours::accent.withAlpha(0.35f));
        setColour(TextEditor::backgroundColourId, UIColours::screen);
        setColour(TextEditor::textColourId, UIColours::readout);
        setColour(TextEditor::outlineColourId, UIColours::transparent);
        setColour(TextEditor::focusedOutlineColourId, UIColours::accent.withAlpha(0.5f));
        setColour(CaretComponent::caretColourId, UIColours::accent);
    }

    // frames stacked top to bottom, frame 0 = fully left
    void setKnobStrip(const Image& strip, int frames)
    {
        _knobStrip = strip;
        _knobFrames = frames;
    }

    // one full turn, frame 0 = pointing up
    void setEncoderStrip(const Image& strip, int frames)
    {
        _encoderStrip = strip;
        _encoderFrames = frames;
    }

    void drawEncoder(Graphics& g, Rectangle<float> bounds, float angle)
    {
        const float r = jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
        const auto c = bounds.getCentre();

        if (_encoderStrip.isValid() && _encoderFrames > 1)
        {
            const int frame = roundToInt(angle / MathConstants<float>::twoPi * (float) _encoderFrames) % _encoderFrames;
            drawFrame(g, _encoderStrip, _encoderFrames, frame, c, r * 0.66f);
            return;
        }

        drawPlainKnob(g, c, r * 0.66f, angle);
    }

    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float pos,
                          float startAngle, float endAngle, Slider&) override
    {
        auto bounds = Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        const float r = jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // lit arc in the milled groove
        if (pos > 0.002f)
        {
            const float k = jmax(0.6f, r / 51.0f);
            const float arcR = r * 0.79f;
            Path arc;
            arc.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
            auto stroke = [](float w) { return PathStrokeType(w, PathStrokeType::curved, PathStrokeType::rounded); };

            g.setColour(UIColours::accent.withAlpha(0.12f));
            g.strokePath(arc, stroke(7.0f * k));
            g.setColour(UIColours::accentDeep);
            g.strokePath(arc, stroke(2.8f * k));
            g.setColour(UIColours::accent.brighter(0.35f).withAlpha(0.9f));
            g.strokePath(arc, stroke(1.1f * k));
        }

        const float kr = r * 0.66f;

        if (_knobStrip.isValid() && _knobFrames > 1)
        {
            // the strip covers the full 270 degree throw
            const float t = (angle - MathConstants<float>::pi * 1.25f) / (MathConstants<float>::pi * 1.5f);
            const int frame = jlimit(0, _knobFrames - 1, roundToInt(t * (float) (_knobFrames - 1)));
            drawFrame(g, _knobStrip, _knobFrames, frame, c, kr);
            return;
        }

        drawPlainKnob(g, c, kr, angle);
    }

    Label* createSliderTextBox(Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox(slider);
        l->setJustificationType(Justification::centred);
        return l;
    }

    Font getLabelFont(Label& label) override
    {
        return UIFonts::mono(jlimit(9.0f, 20.0f, (float) label.getHeight() * 0.68f));
    }

    // readouts are small dot matrix windows behind the glass
    void drawLabel(Graphics& g, Label& label) override
    {
        if (dynamic_cast<Slider*>(label.getParentComponent()) == nullptr)
        {
            LookAndFeel_V4::drawLabel(g, label);
            return;
        }

        if (label.isBeingEdited())
            return;

        // the label is 24 design units tall at the window's current scale
        const float pitch = PanelLayout::dotPitch * (float) label.getHeight() / 24.0f;
        LcdFont::drawWindow(g, label.getLocalBounds().toFloat(), label.getText(), pitch, UIColours::readout);
    }

    // panel print. maskOnly draws plain white, which is how the print mask
    // for the faceplate render is made
    static inline bool maskOnly = false;

    static void print(Graphics& g, const String& text, Rectangle<float> area, const Font& font,
                      Justification just, float alpha = 1.0f)
    {
        g.setFont(font);
        g.setColour(maskOnly ? Colours::white.withAlpha(alpha) : UIColours::ink.withAlpha(0.9f * alpha));
        g.drawText(text, area, just, false);
    }

private:
    // the rendered sprite is 3.2 knob radii wide, shadow included
    static void drawFrame(Graphics& g, const Image& strip, int frames, int frame, Point<float> c, float kr)
    {
        const int fw = strip.getWidth();
        const int fh = strip.getHeight() / frames;
        const float size = kr * 3.2f;

        // full opacity: images take the alpha of the last colour set, and the value arc leaves it at 0.9
        g.setOpacity(1.0f);
        g.setImageResamplingQuality(Graphics::highResamplingQuality);
        g.drawImageTransformed(strip.getClippedImage({ 0, frame * fh, fw, fh }),
                               AffineTransform::scale(size / (float) fw).translated(c.x - size * 0.5f, c.y - size * 0.5f));
    }

    // used if the artwork is missing
    static void drawPlainKnob(Graphics& g, Point<float> c, float kr, float angle)
    {
        g.setColour(UIColours::panel.darker(0.1f));
        g.fillEllipse(Rectangle<float>(kr * 2.0f, kr * 2.0f).withCentre(c));
        g.setColour(UIColours::accentDeep);
        g.drawLine({ c.getPointOnCircumference(kr * 0.3f, angle), c.getPointOnCircumference(kr * 0.9f, angle) }, 2.0f);
    }

    Image _knobStrip, _encoderStrip;
    int _knobFrames = 0, _encoderFrames = 0;
};