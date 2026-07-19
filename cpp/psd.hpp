

// psd.hpp
#pragma once

#include "kissfft/kissfft.hh"
#include <cmath>
#include <complex>
#include <vector>

struct PSDResult {
    std::vector<float> freqsHz;   // absolute frequency (centerFreqHz + baseband offset)
    std::vector<float> powerDb;
};

// nfft: 0 (default) = single periodogram over the whole capture, matching the
// original behavior. nfft > 0 = Welch's method - average power over overlapping
// nfft-sized segments (50% overlap), which trades frequency resolution for a much
// less noisy noise floor, since each bin is now an average of many independent
// estimates instead of one.
inline PSDResult computePSD(const std::vector<std::complex<float>>& iq, double sampleRate,
                             double centerFreqHz = 0.0, size_t nfft = 0) {
    const size_t n = iq.size();
    const size_t fftSize = (nfft == 0 || nfft >= n) ? n : nfft;
    const size_t hop = fftSize / 2;

    // Hann window to reduce spectral leakage (optional but recommended)
    std::vector<float> window(fftSize);
    for (size_t i = 0; i < fftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (fftSize - 1)));

    kissfft<float> fft(fftSize, /*inverse=*/false);
    std::vector<std::complex<float>> segIn(fftSize), segOut(fftSize);
    std::vector<float> accum(fftSize, 0.0f);

    size_t numSegments = 0;
    for (size_t start = 0; start + fftSize <= n; start += hop) {
        for (size_t i = 0; i < fftSize; ++i)
            segIn[i] = iq[start + i] * window[i];
        fft.transform(segIn.data(), segOut.data());
        for (size_t k = 0; k < fftSize; ++k)
            accum[k] += std::norm(segOut[k]);  // |X[k]|^2, cheaper than abs() then squaring
        ++numSegments;
    }

    PSDResult result;
    result.freqsHz.resize(fftSize);
    result.powerDb.resize(fftSize);

    // fftshift: reorder so index 0 = most negative frequency, n-1 = most positive,
    // and compute each bin's frequency relative to center (matches the Fc-centered
    // baseband view from earlier - 0 Hz sits in the middle of the plot)
    for (size_t k = 0; k < fftSize; ++k) {
        size_t shifted = (k + fftSize / 2) % fftSize;
        float basebandFreq = (static_cast<float>(k) - static_cast<float>(fftSize) / 2.0f)
                              * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        float meanMag2 = accum[shifted] / static_cast<float>(numSegments);
        result.freqsHz[k] = static_cast<float>(centerFreqHz) + basebandFreq;
        result.powerDb[k] = 10.0f * std::log10(meanMag2 / static_cast<float>(fftSize) + 1e-20f); // epsilon avoids log(0)
    }

    return result;
}
