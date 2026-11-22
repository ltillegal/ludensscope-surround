#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "Theme.h"
#include "../dsp/Downmix.h"

// Room-view of the six discrete decoded 5.1 channels. The selector row picks
// which channel feeds the FFT, waterfall and oscilloscope; the channel meters
// and peak-hold markers always show all six channels.
class SurroundView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    static constexpr int totalChannels = surround::channelCount + 1; // ALL + 6

    SurroundView()
    {
        setOpaque (true);
        setBufferedToImage (true);
        setTooltip ("5.1 room view. Click a channel to route the FFT, waterfall and scope.");
    }

    std::function<void (int)> onChannelSelect;

    void setData (float* const* channels, int numSamples)
    {
        if (channels == nullptr || numSamples <= 0)
            return;

        for (int c = 0; c < surround::channelCount; ++c)
        {
            const float* data = channels[(size_t) c];

            if (data == nullptr)
                continue;

            double sum = 0.0;
            float peak = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float value = data[i];
                sum += (double) value * (double) value;
                peak = std::max (peak, std::abs (value));
            }

            auto& meter = meters[(size_t) c];
            meter.rmsDb = gainToDb ((float) std::sqrt (sum / (double) numSamples));
            meter.peakDb = gainToDb (peak);
            meter.holdDb = std::max (meter.peakDb, meter.holdDb - 0.6f);
        }

        repaint();
    }

    void setSelectedChannel (int index) noexcept
    {
        selected = juce::jlimit (0, surround::channelCount, index);
        repaint();
    }

    int getSelectedChannel() const noexcept { return selected; }

    // Used by the UI snapshot test to prove meters keep moving while FREEZE
    // holds only the spectrum and waterfall.
    float getPeakDb (int channel) const noexcept
    {
        const auto index = (size_t) juce::jlimit (0, surround::channelCount - 1, channel);
        return meters[index].peakDb;
    }

    void setLocked (bool shouldBeLocked) noexcept
    {
        if (locked == shouldBeLocked)
            return;

        locked = shouldBeLocked;
        repaint();
    }

    void reset() noexcept
    {
        for (auto& meter : meters)
            meter = {};
        repaint();
    }

    void setUiScale (float scale) noexcept
    {
        const float next = juce::jlimit (0.75f, 1.5f, scale);

        if (std::abs (next - uiScale) < 0.001f)
            return;

        uiScale = next;
        repaint();
    }

    float getUiScale() const noexcept { return uiScale; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        theme::drawPanel (g, bounds);

        auto area = bounds.reduced (juce::roundToInt (8.0f * uiScale));

        const int headerHeight = juce::roundToInt (13.0f * uiScale);
        auto header = area.removeFromTop ((float) headerHeight).toNearestInt();

        g.setFont (theme::detailFont (9.5f, uiScale));
        g.setColour (locked ? theme::textDim : theme::text);
        g.drawFittedText ("SURROUND 5.1", header, juce::Justification::centredLeft, 1, 0.6f);

        const auto badge = header.removeFromRight (juce::roundToInt (28.0f * uiScale));
        g.setColour (locked ? theme::hot : theme::accent);
        g.fillRoundedRectangle (badge.toFloat().reduced (0.0f, 1.0f), 3.0f);
        g.setColour (theme::bg);
        g.setFont (theme::detailBoldFont (8.5f, uiScale));
        g.drawText (locked ? "PRO" : "5.1", badge, juce::Justification::centred);

        area.removeFromBottom ((float) juce::roundToInt (17.0f * uiScale));
        auto selector = bounds.toNearestInt().removeFromBottom (juce::roundToInt (17.0f * uiScale))
                             .reduced (juce::roundToInt (8.0f * uiScale), 0);

        drawSelector (g, selector);
        drawRoom (g, area);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (selectorBounds[0].isEmpty())
            return;

        for (int i = 0; i < totalChannels; ++i)
        {
            if (selectorBounds[(size_t) i].contains (e.getPosition()))
            {
                if (onChannelSelect != nullptr)
                    onChannelSelect (i);
                return;
            }
        }
    }

private:
    struct ChannelMeter
    {
        float rmsDb = -100.0f;
        float peakDb = -100.0f;
        float holdDb = -100.0f;
    };

    static float gainToDb (float gain) noexcept
    {
        return gain > 1.0e-6f ? juce::jlimit (-60.0f, 6.0f,
                                              20.0f * (float) std::log10 ((double) gain))
                              : -60.0f;
    }

    static const char* channelName (int index) noexcept
    {
        static const char* names[] = { "L", "R", "C", "LFE", "Ls", "Rs" };
        return names[juce::jlimit (0, 5, index)];
    }

    static juce::Colour channelColour (int index) noexcept
    {
        switch (index)
        {
            case 0: return theme::accent;
            case 1: return theme::accent2;
            case 2: return theme::cold;
            case 3: return theme::textDim;
            case 4: return theme::hot;
            default: return theme::hot.brighter (0.25f);
        }
    }

    void drawSelector (juce::Graphics& g, juce::Rectangle<int> area)
    {
        selectorBounds = {};

        const int gap = juce::jmax (1, juce::roundToInt (1.5f * uiScale));
        const int width = (area.getWidth() - gap * (totalChannels - 1)) / totalChannels;
        const int height = juce::jmin (area.getHeight(), juce::roundToInt (15.0f * uiScale));

        for (int i = 0; i < totalChannels; ++i)
        {
            auto button = area.removeFromLeft (i == totalChannels - 1 ? area.getWidth() : width);
            button.setHeight (height);
            selectorBounds[(size_t) i] = button;

            const bool active = i == selected;
            const auto colour = i == 0 ? theme::text : channelColour (i - 1);

            g.setColour (active ? colour : theme::panel);
            g.fillRoundedRectangle (button.toFloat(), 3.0f);
            g.setColour (active ? colour.withAlpha (0.9f) : theme::panelLine);
            g.drawRoundedRectangle (button.toFloat().reduced (0.5f), 3.0f, 1.0f);

            g.setFont (theme::detailFont (i == 0 ? 7.5f : 8.0f, uiScale));
            g.setColour (active ? theme::bg : (locked ? theme::textDim : colour));
            g.drawText (i == 0 ? "ALL" : channelName (i - 1), button,
                        juce::Justification::centred);

            area.removeFromLeft (gap);
        }
    }

    void drawRoom (juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f)
            return;

        const auto room = area.reduced (2.0f);

        g.setColour (theme::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (room, 6.0f);
        g.setColour (theme::grid);
        g.drawRoundedRectangle (room, 6.0f, 1.0f);

        const auto centre = juce::Point<float> (room.getCentreX(), room.getY() + room.getHeight() * 0.56f);
        const float radius = juce::jmin (room.getWidth() * 0.42f, room.getHeight() * 0.42f);

        g.setColour (theme::gridStrong.withAlpha (0.55f));
        g.drawLine (centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);
        g.drawLine (centre.x, centre.y - radius * 0.7f, centre.x, centre.y + radius * 0.7f, 1.0f);

        g.setColour (theme::textDim);
        g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);

        // L, R, C, Ls, Rs placed as seen from the listening position; LFE sits
        // near the front centre and is drawn smaller because it is not measured
        // by the BS.1770 meter.
        const juce::Point<float> positions[surround::channelCount] = {
            { room.getX() + room.getWidth() * 0.18f, room.getY() + room.getHeight() * 0.32f },
            { room.getX() + room.getWidth() * 0.82f, room.getY() + room.getHeight() * 0.32f },
            { room.getX() + room.getWidth() * 0.50f, room.getY() + room.getHeight() * 0.18f },
            { room.getX() + room.getWidth() * 0.50f, room.getY() + room.getHeight() * 0.72f },
            { room.getX() + room.getWidth() * 0.17f, room.getY() + room.getHeight() * 0.82f },
            { room.getX() + room.getWidth() * 0.83f, room.getY() + room.getHeight() * 0.82f }
        };

        for (int c = 0; c < surround::channelCount; ++c)
        {
            const auto& meter = meters[(size_t) c];
            const float level = juce::jlimit (0.0f, 1.0f, (meter.rmsDb + 60.0f) / 60.0f);
            const float baseSize = c == surround::lfe ? 3.0f : 4.5f;
            const float size = baseSize + level * (c == surround::lfe ? 2.0f : 4.5f) * uiScale;
            const auto point = positions[(size_t) c];
            const bool active = selected == c + 1;
            const auto colour = channelColour (c);

            if (meter.holdDb > -59.0f)
            {
                const float holdSize = size + 3.0f * uiScale;
                g.setColour (colour.withAlpha (locked ? 0.20f : 0.45f));
                g.drawEllipse (point.x - holdSize * 0.5f, point.y - holdSize * 0.5f,
                               holdSize, holdSize, 1.0f);
            }

            g.setColour (locked ? theme::textDim.withAlpha (0.45f) : colour.withAlpha (0.25f + 0.65f * level));
            g.fillEllipse (point.x - size * 0.5f, point.y - size * 0.5f, size, size);

            if (active)
            {
                g.setColour (theme::text);
                g.drawEllipse (point.x - size - 2.0f, point.y - size - 2.0f,
                               size * 2.0f + 4.0f, size * 2.0f + 4.0f, 1.0f);
            }

            const auto label = juce::Rectangle<float> (point.x - 13.0f, point.y - 18.0f, 26.0f, 9.0f);
            g.setFont (theme::detailFont (7.5f, uiScale));
            g.setColour (locked ? theme::textDim : colour);
            g.drawText (channelName (c), label, juce::Justification::centred);

            const auto dbLabel = juce::Rectangle<float> (point.x - 15.0f, point.y + 7.0f, 30.0f, 9.0f);
            g.setFont (theme::detailFont (6.5f, uiScale));
            g.setColour (locked ? theme::textDim : theme::text.withAlpha (0.75f));
            g.drawText (meter.peakDb <= -59.0f ? juce::String ("-inf")
                                               : juce::String (meter.peakDb, 0),
                        dbLabel, juce::Justification::centred);
        }
    }

    std::array<ChannelMeter, surround::channelCount> meters {};
    std::array<juce::Rectangle<int>, totalChannels> selectorBounds {};
    int selected = 0;
    bool locked = false;
    float uiScale = 1.0f;
};
