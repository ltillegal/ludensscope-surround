#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "Theme.h"
#include "../dsp/Downmix.h"

// Nuendo-style bargraph view of the six discrete decoded 5.1 channels. The
// selector row (or clicking a strip) picks which channel feeds the FFT,
// waterfall and oscilloscope; all six RMS/peak meters stay live.
class SurroundView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    static constexpr int totalChannels = surround::channelCount + 1; // ALL + 6

    SurroundView()
    {
        setOpaque (true);
        setBufferedToImage (true);
        setTooltip ("5.1 channel meters. Click a strip to route the FFT, waterfall and scope. "
                    "LFE uses a +10 dB display trim.");
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

            // The room view is read at a glance, so let its numbers and bubble
            // sizes settle instead of jumping on every 30 Hz editor tick.
            // The raw peakDb still feeds getPeakDb() and the peak-hold ring.
            constexpr float displaySmoothing = 0.08f;

            meter.shownRmsDb = meter.shownRmsDb <= -99.0f
                                   ? meter.rmsDb
                                   : meter.shownRmsDb
                                         + displaySmoothing * (meter.rmsDb - meter.shownRmsDb);
            meter.shownPeakDb = meter.shownPeakDb <= -99.0f
                                    ? meter.peakDb
                                    : meter.shownPeakDb
                                          + displaySmoothing * (meter.peakDb - meter.shownPeakDb);

            // Nuendo-style peak hold: keep the marker still for ~1.5 s, then
            // let it fall at 18 dB/s. Clip LEDs latch for ~2 s.
            static constexpr int ticksPerSecond = 30;
            static constexpr int holdTicks = 3 * ticksPerSecond / 2;
            static constexpr int clipTicks = 2 * ticksPerSecond;

            if (meter.peakDb >= meter.holdDb - 0.1f)
            {
                meter.holdDb = meter.peakDb;
                meter.holdCountdown = holdTicks;
            }
            else if (meter.holdCountdown > 0)
                --meter.holdCountdown;
            else
                meter.holdDb = juce::jmax (meter.rmsDb, meter.holdDb - 0.6f);

            if (meter.peakDb >= -0.2f)
                meter.clipCountdown = clipTicks;
            else if (meter.clipCountdown > 0)
                --meter.clipCountdown;
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
        drawMeters (g, area);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < totalChannels; ++i)
        {
            if (selectorBounds[(size_t) i].contains (e.getPosition()))
            {
                if (onChannelSelect != nullptr)
                    onChannelSelect (i);
                return;
            }
        }

        for (int c = 0; c < surround::channelCount; ++c)
        {
            if (meterBounds[(size_t) c].contains (e.getPosition().toFloat()))
            {
                if (onChannelSelect != nullptr)
                    onChannelSelect (c + 1);
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
        float shownRmsDb = -100.0f;
        float shownPeakDb = -100.0f;
        int holdCountdown = 0;
        int clipCountdown = 0;
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

    void drawMeters (juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (area.getWidth() <= 12.0f || area.getHeight() <= 12.0f)
            return;

        const auto box = area.reduced (2.0f);

        g.setColour (theme::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (theme::grid);
        g.drawRoundedRectangle (box, 6.0f, 1.0f);

        meterBounds = {};

        const float scaleW = juce::jmax (13.0f, 16.0f * uiScale);
        const float bottomH = juce::jmax (16.0f, 18.0f * uiScale);
        const float clipH = juce::jmax (5.0f, 6.0f * uiScale);
        const float pad = 3.0f;

        const float trackX = box.getX() + scaleW + pad;
        const float trackY = box.getY() + clipH + pad;
        const float trackW = juce::jmax (1.0f, box.getRight() - pad - trackX);
        const float trackH = juce::jmax (8.0f, box.getBottom() - bottomH - pad - trackY);

        // Shared dB ruler. LFE is trimmed +10 dB for display only (marked in
        // the tooltip): a correctly calibrated LFE then reads like the other
        // channels, while its numeric label and clip LED stay in real dBFS.
        const float ticks[] = { 0.0f, -12.0f, -24.0f, -48.0f };

        for (float db : ticks)
        {
            const float y = yForDb (db, trackY, trackH);

            g.setColour (theme::gridStrong.withAlpha (0.7f));
            g.drawLine (box.getX() + 2.0f, y, trackX - 2.0f, y, 1.0f);

            g.setFont (theme::detailFont (6.5f, uiScale));
            g.setColour (theme::textDim);
            g.drawText (juce::String (db, 0),
                        juce::Rectangle<float> (box.getX() + 1.0f, y - 5.0f, scaleW - 4.0f, 10.0f),
                        juce::Justification::centredRight);
        }

        const float gap = juce::jmax (1.0f, 2.0f * uiScale);
        const float barW = juce::jmax (2.0f, (trackW - gap * (surround::channelCount - 1))
                                              / (float) surround::channelCount);

        for (int c = 0; c < surround::channelCount; ++c)
        {
            const auto& meter = meters[(size_t) c];
            const float trim = c == surround::lfe ? 10.0f : 0.0f;
            const float x = trackX + (float) c * (barW + gap);
            const bool active = selected == c + 1;

            meterBounds[(size_t) c] = juce::Rectangle<float> (x - gap * 0.5f, box.getY(),
                                                              barW + gap, box.getHeight());

            // Clip indicator, latched in setData from the untrimmed peak.
            auto clipRect = juce::Rectangle<float> (x, box.getY() + 1.0f, barW, clipH - 2.0f);
            g.setColour (meter.clipCountdown > 0 ? theme::hot : theme::panelLine.withAlpha (0.45f));
            g.fillRoundedRectangle (clipRect, 1.5f);

            // Meter well.
            g.setColour (theme::bg.withAlpha (0.65f));
            g.fillRect (x, trackY, barW, trackH);

            const float rmsY = yForDb (meter.shownRmsDb + trim, trackY, trackH);
            const float fillH = trackY + trackH - rmsY;

            if (fillH > 0.5f)
            {
                juce::ColourGradient fill (theme::accent, x, trackY + trackH,
                                           theme::hot, x, trackY, false);
                fill.addColour (0.62, theme::accent2);

                g.setGradientFill (fill);
                g.fillRect (x, rmsY, barW, fillH);
            }

            // Instantaneous peak cap and the delayed peak-hold marker.
            const float peakY = yForDb (meter.peakDb + trim, trackY, trackH);
            g.setColour (theme::text.withAlpha (0.85f));
            g.fillRect (x, peakY, barW, juce::jmax (1.0f, 1.5f * uiScale));

            if (meter.holdDb > -59.0f)
            {
                const float holdY = yForDb (meter.holdDb + trim, trackY, trackH);
                g.setColour (meter.peakDb >= -1.0f ? theme::hot : theme::accent2);
                g.fillRect (x, holdY - 0.5f, barW, juce::jmax (1.0f, 1.5f * uiScale));
            }

            if (active)
            {
                g.setColour (theme::text.withAlpha (0.9f));
                g.drawRect (x - 0.5f, trackY - 0.5f, barW + 1.0f, trackH + 1.0f, 1.0f);
            }

            const auto labelRect = juce::Rectangle<float> (x - 1.0f, box.getBottom() - bottomH,
                                                           barW + 2.0f, 10.0f);
            g.setFont (theme::detailBoldFont (7.5f, uiScale));
            g.setColour (active ? theme::text : channelColour (c));
            g.drawFittedText (channelName (c), labelRect.toNearestInt(),
                              juce::Justification::centred, 1, 0.55f);

            const auto valueRect = juce::Rectangle<float> (x - 2.0f, box.getBottom() - bottomH + 9.0f,
                                                           barW + 4.0f, 9.0f);
            g.setFont (theme::detailFont (6.0f, uiScale));
            g.setColour (theme::text.withAlpha (0.75f));
            g.drawFittedText (meter.shownPeakDb <= -59.0f ? juce::String ("-inf")
                                                         : juce::String (meter.shownPeakDb, 0),
                              valueRect.toNearestInt(), juce::Justification::centred, 1, 0.55f);
        }

        if (locked)
        {
            g.setColour (theme::bg.withAlpha (0.45f));
            g.fillRoundedRectangle (box, 6.0f);
        }
    }

    static float yForDb (float db, float top, float height) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, (6.0f - db) / 66.0f);
        return top + t * height;
    }

    std::array<ChannelMeter, surround::channelCount> meters {};
    std::array<juce::Rectangle<int>, totalChannels> selectorBounds {};
    std::array<juce::Rectangle<float>, surround::channelCount> meterBounds {};
    int selected = 0;
    bool locked = false;
    float uiScale = 1.0f;
};
