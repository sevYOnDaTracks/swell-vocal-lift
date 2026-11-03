#pragma once
#include <algorithm>
#include <cmath>

// Complementary bands and a stereo-linked, relative sibilance detector.
// All coefficients are prepared off the audio thread. No allocation in tick().
class LiftDSP
{
    struct Channel { float low = 0, mid = 0, high = 0; } filters[2];
    float lowCoefficient = 0, midCoefficient = 0, highCoefficient = 0;
    float attackCoefficient = 0, releaseCoefficient = 0;
    float fullEnvelope = 0, highEnvelope = 0;

public:
    void prepare(double sampleRate)
    {
        auto lowpass = [sampleRate](double hz)
        { return float(1.0 - std::exp(-2.0 * 3.141592653589793 * hz / sampleRate)); };
        lowCoefficient = lowpass(180);
        midCoefficient = lowpass(1800);
        highCoefficient = lowpass(5500);
        attackCoefficient = float(std::exp(-1.0 / (0.001 * sampleRate)));
        releaseCoefficient = float(std::exp(-1.0 / (0.060 * sampleRate)));
        reset();
    }

    void reset()
    {
        filters[0] = {}; filters[1] = {};
        fullEnvelope = highEnvelope = 0;
    }

    void tick(float* samples, int channels, float amount)
    {
        const float lift = std::clamp(amount, 0.0f, 1.0f);
        float air[2]{}, presence[2]{}, low[2]{};
        float fullPeak = 0, highPeak = 0;
        for (int channel = 0; channel < channels; ++channel)
        {
            const float input = samples[channel];
            auto& state = filters[channel];
            state.low += lowCoefficient * (input - state.low);
            state.mid += midCoefficient * (input - state.mid);
            state.high += highCoefficient * (input - state.high);
            low[channel] = state.low;
            presence[channel] = state.high - state.mid;
            air[channel] = input - state.high;
            fullPeak = std::max(fullPeak, std::abs(input));
            highPeak = std::max(highPeak, std::abs(air[channel]));
        }
        auto follow = [this](float input, float envelope)
        {
            const float coefficient = input > envelope ? attackCoefficient : releaseCoefficient;
            return coefficient * envelope + (1.0f - coefficient) * input;
        };
        fullEnvelope = follow(fullPeak, fullEnvelope);
        highEnvelope = follow(highPeak, highEnvelope);
        // The small floor prevents quiet noise from triggering the detector.
        const float ratio = highEnvelope / (fullEnvelope + 0.003f);
        const float sibilance = std::clamp((ratio - 0.28f) / 0.18f, 0.0f, 1.0f);
        const float highGain = (1.0f + 1.25f * lift) * (1.0f - 0.70f * lift * sibilance);
        const float compensation = 1.0f / (1.0f + 0.12f * lift);
        for (int channel = 0; channel < channels; ++channel)
        {
            const float input = samples[channel];
            // A small parallel harmonic contribution is confined to the presence band.
            const float excitation = 0.08f * lift * lift
                * (std::tanh(presence[channel] * 3.0f) / 3.0f - presence[channel]);
            const float shaped = input - 0.10f * lift * low[channel]
                + 0.75f * lift * presence[channel] + (highGain - 1.0f) * air[channel]
                + excitation;
            samples[channel] = lift == 0.0f ? input : shaped * compensation;
        }
    }
};
