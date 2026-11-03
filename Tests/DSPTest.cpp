#include "../Source/LiftDSP.h"
#include <iostream>
#include <limits>

double gainAt(double frequency, double rate, float amount)
{
    LiftDSP dsp; dsp.prepare(rate);
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < int(rate * .4); ++i)
    {
        float input = float(.12 * std::sin(2 * 3.141592653589793 * frequency * i / rate));
        float frame[2]{input, input}; dsp.tick(frame, 2, amount);
        if (i > rate * .2) { inputEnergy += input * input; outputEnergy += frame[0] * frame[0]; }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

int main()
{
    for (double rate : {44100., 48000., 96000., 192000., 384000.})
    {
        for (float amount : {0.f, .25f, .70f, 1.f})
        {
            LiftDSP dsp; dsp.prepare(rate);
            for (int i = 0; i < 48000; ++i)
            {
                float input = float(.3 * std::sin(i * .17) + .1 * std::sin(i * 1.7));
                float frame[2]{input, input}; dsp.tick(frame, 2, amount);
                if (!std::isfinite(frame[0]) || std::abs(frame[0]) > 2 || frame[0] != frame[1]) return 1;
                if (amount == 0 && frame[0] != input) return 2;
            }
            dsp.reset();
            for (int i = 0; i < 1000; ++i)
            { float frame[2]{}; dsp.tick(frame, 2, amount); if (frame[0] != 0 || frame[1] != 0) return 3; }
        }
        double presence = gainAt(2800, rate, 1);
        double sibilant = gainAt(9000, rate, 1);
        double bass = gainAt(100, rate, 1);
        std::cout << rate << " Hz: presence=" << presence << ", sibilant=" << sibilant << ", bass=" << bass << '\n';
        // Brightness must not indiscriminately boost isolated sibilant energy.
        if (presence < 1.02 || sibilant > 1.05 || bass > 1.02) return 4;
    }
    // Silence in the right channel must stay silent while left-channel audio plays.
    LiftDSP dsp; dsp.prepare(192000);
    for (int i=0; i<10000; ++i)
    { float frame[2]{float(.2 * std::sin(i * .1)),0}; dsp.tick(frame,2,1); if(frame[1]!=0)return 5; }
    std::cout << "PASS: zero unity, silence, finite output, stereo link, no crosstalk and spectral response.\n";
}
