// fm_demod.hpp
#pragma once

#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

// Mono WBFM broadcast: +-75 kHz deviation + 15 kHz audio bandwidth. Carson's
// rule puts occupied bandwidth at ~180 kHz, so we filter/decimate down to
// something comfortably wider than that rather than all the way to audio rate.
constexpr double kFmChannelHalfBw = 100e3;
constexpr double kMaxDeviation = 75e3;

inline float sincNorm(double x) {
    // normalized sinc: sin(pi*x)/(pi*x), sinc(0) = 1
    if (std::abs(x) < 1e-9) return 1.0f;
    double px = M_PI * x;
    return static_cast<float>(std::sin(px) / px);
}

// Windowed-sinc lowpass FIR, unity DC gain. Mirrors scipy.signal.firwin.
inline std::vector<float> designLowpassFir(int numTaps, double cutoffHz, double sampleRate) {
    std::vector<float> taps(numTaps);
    const double center = (numTaps - 1) / 2.0;
    const double fcNorm = cutoffHz / sampleRate;  // fraction of sample rate

    double sum = 0.0;
    for (int i = 0; i < numTaps; ++i) {
        double n = i - center;
        double ideal = 2.0 * fcNorm * sincNorm(2.0 * fcNorm * n);
        double hamming = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (numTaps - 1));
        taps[i] = static_cast<float>(ideal * hamming);
        sum += taps[i];
    }
    for (auto& t : taps) t = static_cast<float>(t / sum);  // normalize to unity DC gain
    return taps;
}

// Causal FIR convolution, y[n] = sum_k h[k]*x[n-k], zero history before x[0].
// Same shape/semantics as scipy.signal.lfilter(taps, 1.0, x).
inline std::vector<std::complex<float>> filterComplex(const std::vector<float>& taps,
                                                        const std::vector<std::complex<float>>& x) {
    std::vector<std::complex<float>> y(x.size());
    for (size_t n = 0; n < x.size(); ++n) {
        std::complex<float> acc{0.0f, 0.0f};
        size_t kMax = std::min(taps.size(), n + 1);
        for (size_t k = 0; k < kMax; ++k) acc += taps[k] * x[n - k];
        y[n] = acc;
    }
    return y;
}

// Quadrature discriminator: phase difference between consecutive samples,
// normalized so full-scale deviation maps to +-1.
inline std::vector<float> fmDemodulate(const std::vector<std::complex<float>>& iq, double sampleRate) {
    std::vector<float> audio(iq.size() > 0 ? iq.size() - 1 : 0);
    for (size_t n = 0; n < audio.size(); ++n) {
        std::complex<float> prod = iq[n + 1] * std::conj(iq[n]);
        double phaseDiff = std::atan2(prod.imag(), prod.real());
        audio[n] = static_cast<float>(phaseDiff * sampleRate / (2.0 * M_PI * kMaxDeviation));
    }
    return audio;
}

// Rational resampler (polyphase), equivalent to scipy.signal.resample_poly(x, up, down).
// Upsamples by `up`, lowpass filters to prevent both imaging and aliasing, downsamples
// by `down` - without ever materializing the zero-stuffed intermediate signal.
inline std::vector<float> resamplePoly(const std::vector<float>& x, int upIn, int downIn, double sampleRateIn) {
    int g = std::gcd(upIn, downIn);
    int up = upIn / g;
    int down = downIn / g;
    double sampleRateOut = sampleRateIn * up / down;

    // Anti-aliasing/anti-imaging filter, designed at the (virtual) upsampled rate.
    const double sampleRateUp = sampleRateIn * up;
    const double cutoffHz = 0.5 * std::min(sampleRateIn, sampleRateOut);
    const int tapsPerPhase = 16;  // taps per polyphase branch - trades stopband for compute
    const int protoLen = tapsPerPhase * 2 * up + 1;
    std::vector<float> proto = designLowpassFir(protoLen, cutoffHz, sampleRateUp);
    for (auto& t : proto) t *= static_cast<float>(up);  // restore gain lost to zero-stuffing

    // e_phase[j] = proto[phase + j*up]: polyphase decomposition of the prototype filter.
    std::vector<std::vector<float>> phases(up);
    for (int p = 0; p < up; ++p) {
        for (int k = p; k < protoLen; k += up) phases[p].push_back(proto[k]);
    }

    long long numOut = (static_cast<long long>(x.size()) * up) / down;
    std::vector<float> y(numOut);
    for (long long m = 0; m < numOut; ++m) {
        long long n = m * down;
        int phase = static_cast<int>(n % up);
        long long i0 = n / up;
        const auto& e = phases[phase];
        float acc = 0.0f;
        for (size_t j = 0; j < e.size(); ++j) {
            long long i = i0 - static_cast<long long>(j);
            if (i >= 0 && i < static_cast<long long>(x.size())) acc += e[j] * x[i];
        }
        y[m] = acc;
    }
    return y;
}
