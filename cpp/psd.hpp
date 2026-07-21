

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

// Hann window to reduce spectral leakage
inline std::vector<float> compute_hann_window(const size_t fftSize)
{
    std::vector<float> window(fftSize);
    for (size_t i = 0; i < fftSize; ++i)
    {
        const float k2PI = 2.0f * M_PI;
        window[i] = 0.5f * (1.0f - std::cos(k2PI * i / (fftSize - 1)));
    }
    return window;
}

inline int fft_hann_accum(const std::vector<std::complex<float>>& iq, 
                          const std::vector<float> &window, 
                          std::vector<float> &accum, 
                          const size_t hop
)
{
    auto fftSize = accum.size();
    kissfft<float> fft(fftSize, /*inverse=*/false);
    std::vector<std::complex<float>> segIn(fftSize), segOut(fftSize);

    size_t numSegments = 0;
    for (size_t start = 0; start + fftSize <= iq.size(); start += hop) 
    {
        for (size_t i = 0; i < fftSize; ++i) 
        { 
            segIn[i] = iq[start + i] * window[i];
        }
        fft.transform(segIn.data(), segOut.data());
        for (size_t k = 0; k < fftSize; ++k) 
        {
            // accumulate the power value for each bin (1st part of Welch's method)
            accum[k] += std::norm(segOut[k]);  // |X[k]|^2, cheaper than abs() then squaring
        }
        ++numSegments;
    }
    return numSegments;

}

// nfft: 0 (default) = single periodogram over the whole capture, matching the
// original behavior. nfft > 0 = Welch's method - average power over overlapping
// nfft-sized segments (50% overlap), which trades frequency resolution for a much
// less noisy noise floor, since each bin is now an average of many independent
// estimates instead of one.
inline PSDResult computePSD(const std::vector<std::complex<float>>& iq, double sampleRate,
                             double centerFreqHz = 0.0, size_t nfft = 0) {

    // Samples per segment in the time domain (equivalent to the number of bins in the FFT output). 
    const size_t fftSize = (nfft == 0 || nfft >= iq.size()) ? iq.size() : nfft;

    // The sample overlap used in Welch's method: 50% of the sample per segment count.
    const size_t hop = fftSize / 2;

    std::vector<float> window = compute_hann_window(fftSize);

    std::vector<float> accum(fftSize, 0.0f);
    size_t numSegments = fft_hann_accum(iq, window, accum, hop);

    PSDResult result;
    result.freqsHz.resize(fftSize);
    result.powerDb.resize(fftSize);

    // fftshift: reorder so index 0 = most negative frequency, n-1 = most positive,
    // and compute each bin's frequency relative to center (matches the Fc-centered
    // baseband view from earlier - 0 Hz sits in the middle of the plot)
    for (size_t k = 0; k < fftSize; ++k) 
    {
        // raw FFT output is: [ DC | Positive freq bins | Negative freq bins ]
        // So we must offset and then wrap around to get the readable left-to-right order.
        size_t shifted = (k + fftSize / 2) % fftSize;

        // Recenter the bin index around zero and get the frequency at that index
        const float kBinBandwidthHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        const float kRecenteredBinIndex =  (static_cast<float>(k) - static_cast<float>(fftSize) / 2.0f);
        float basebandFreq = kRecenteredBinIndex * kBinBandwidthHz;

        // take the average of the accumulated power values (2nd part of Welch's method)
        float meanMag2 = accum[shifted] / static_cast<float>(numSegments);

        // Normalize the power value and convert to DB
        result.powerDb[k] = 10.0f * std::log10(meanMag2 / static_cast<float>(fftSize) + 1e-20f); // epsilon avoids log(0)

        // adjust to requested center freq
        result.freqsHz[k] = centerFreqHz + basebandFreq;
    }

    return result;
}
