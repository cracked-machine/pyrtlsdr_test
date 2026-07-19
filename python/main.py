"""Connect to a local rtl_tcp server, grab a burst of IQ, plot PSD + spectrogram.

Usage:
    rtl_tcp &
    python capture_psd.py [--freq 100e6] [--rate 2.4e6] [--gain 40] [--n 2**20]
"""

import argparse

import numpy as np

from rtltcp import RtlTcpClient


def compute_psd(iq, sample_rate, center_freq_hz, nfft=None):
    """Hann-windowed FFT PSD. Returns (freq_hz, power_db), matching cpp/psd.hpp.

    nfft=None (default): single periodogram over the whole capture.
    nfft=N: Welch's method - average power over overlapping N-sized segments
    (50% overlap), trading frequency resolution for a much less noisy noise
    floor, since each bin becomes an average of many independent estimates.
    """
    n = len(iq)
    fft_size = n if not nfft or nfft >= n else nfft
    hop = fft_size // 2
    window = np.hanning(fft_size)

    accum = np.zeros(fft_size)
    num_segments = 0
    for start in range(0, n - fft_size + 1, hop):
        segment = iq[start : start + fft_size] * window
        accum += np.abs(np.fft.fft(segment)) ** 2
        num_segments += 1

    mean_mag2 = accum / num_segments
    power_db = 10 * np.log10(np.fft.fftshift(mean_mag2) / fft_size + 1e-20)
    freq_hz = center_freq_hz + np.fft.fftshift(np.fft.fftfreq(fft_size, d=1 / sample_rate))
    return freq_hz, power_db


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=1234)
    p.add_argument("--freq", type=float, default=100e6, help="center frequency in Hz")
    p.add_argument("--rate", type=float, default=2.4e6, help="sample rate in Hz")
    p.add_argument("--gain", type=float, default=None, help="tuner gain in dB (omit for AGC)")
    p.add_argument("--n", type=int, default=2**20, help="number of IQ samples to capture. Default: 1,048,576")
    p.add_argument("--out", default="iq.dat", help="output file for raw IQ samples")
    p.add_argument("--psd-out", default="psd.dat", help="output file for computed PSD (freq_hz, power_db)")
    p.add_argument("--nfft", type=int, default=None,
                    help="segment size for Welch's method (omit for a single periodogram over the whole capture)")
    args = p.parse_args()

    with RtlTcpClient(host=args.host, port=args.port) as sdr:
        sdr.set_sample_rate(args.rate)
        sdr.set_center_freq(args.freq)
        if args.gain is None:
            sdr.set_gain_mode(False)
            sdr.set_agc_mode(True)
        else:
            sdr.set_gain_mode(True)
            sdr.set_gain(int(args.gain * 10))

        # drop the first chunk - tuner needs a moment to settle after retuning
        sdr.read_samples(int(args.rate * 0.2))
        samples = sdr.read_samples(args.n)

        # raw interleaved float32 I/Q
        samples.astype(np.complex64).tofile(args.out)
        print(f"wrote {len(samples)} samples to {args.out}")

        # compute PSD (freq_hz, power_db columns)
        freq_hz, power_db = compute_psd(samples, args.rate, args.freq, args.nfft)
        np.savetxt(args.psd_out, np.column_stack([freq_hz, power_db]))
        print(f"wrote PSD to {args.psd_out}")

if __name__ == "__main__":
    main()
