#pragma once

#include "JuceHeader.h"
#include "BinaryData.h"

namespace UIFonts
{
	inline juce::Typeface::Ptr getTypeface()
	{
		static auto typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::MajorMonoDisplayRegular_ttf, BinaryData::MajorMonoDisplayRegular_ttfSize);

		return typeface;
	}

	// the plugin name
	inline juce::Font name(float size)
	{
		return juce::Font(juce::FontOptions(size).withTypeface(getTypeface()));
	}

	// printed panel labels
	inline juce::Font label(float size)
	{
		return juce::Font(juce::FontOptions(size).withStyle("Bold")).withExtraKerningFactor(0.06f);
	}

	// readouts and scope text
	inline juce::Font mono(float size)
	{
		return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
	}
}