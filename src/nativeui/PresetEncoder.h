#pragma once

#include "JuceHeader.h"
#include "CustomLookAndFeel.h"

// Endless rotary encoder for the presets. It has no value of its own,
// it just reports a step each time it clicks round.
class PresetEncoder : public Component
{
public:
    std::function<void (int)> onStep;

    void paint (Graphics& g) override
    {
        if (auto* lf = dynamic_cast<PanelLookAndFeel*> (&getLookAndFeel()))
            lf->drawEncoder (g, getLocalBounds().toFloat(), _angle);
    }

    void mouseDown (const MouseEvent&) override { _dragSteps = 0; }

    void mouseDrag (const MouseEvent& e) override
    {
        // one click per 12 pixels, dragging up or right turns it clockwise
        const int steps = (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 12;
        while (_dragSteps < steps) { ++_dragSteps; turn (1); }
        while (_dragSteps > steps) { --_dragSteps; turn (-1); }
    }

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        // a mouse wheel notch is one click, however big a step the system reports for it.
        // trackpads send lots of small moves, so those add up to a notch's worth first
        if (! wheel.isSmooth)
        {
            if (wheel.deltaY != 0.0f)
                turn (wheel.deltaY > 0.0f ? 1 : -1);
            return;
        }

        _wheel += wheel.deltaY;
        while (_wheel >= 0.25f)  { _wheel -= 0.25f; turn (1); }
        while (_wheel <= -0.25f) { _wheel += 0.25f; turn (-1); }
    }

private:
    void turn (int direction)
    {
        const float twoPi = MathConstants<float>::twoPi;
        _angle = std::fmod (_angle + (float) direction * twoPi / 24.0f + twoPi, twoPi);
        repaint();

        if (onStep)
            onStep (direction);
    }

    float _angle = 0.0f, _wheel = 0.0f;
    int _dragSteps = 0;
};