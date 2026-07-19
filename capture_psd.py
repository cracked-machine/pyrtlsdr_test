"""Connect to a local rtl_tcp server, grab a burst of IQ, plot PSD + spectrogram.

Usage:
    rtl_tcp &
    python capture_psd.py [--freq 100e6] [--rate 2.4e6] [--gain 40] [--n 2**20]
"""

import argparse

import matplotlib.pyplot as plt
import numpy as np

from rtltcp import RtlTcpClient


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=1234)
    p.add_argument("--freq", type=float, default=100e6, help="center frequency in Hz")
    p.add_argument("--rate", type=float, default=2.4e6, help="sample rate in Hz")
    p.add_argument("--gain", type=float, default=None, help="tuner gain in dB (omit for AGC)")
    p.add_argument("--n", type=int, default=2**20, help="number of IQ samples to capture")
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
        sdr.read_samples(int(args.rate * 0.1))
        samples = sdr.read_samples(args.n)

    fig, (ax_psd, ax_spec) = plt.subplots(2, 1, figsize=(10, 8))

    ax_psd.psd(samples, NFFT=2048, Fs=args.rate / 1e6, Fc=args.freq / 1e6)
    ax_psd.set_xlabel("Frequency (MHz)")
    ax_psd.set_ylabel("PSD (dB/Hz)")
    ax_psd.set_title(f"PSD @ {args.freq/1e6:.3f} MHz, {args.rate/1e6:.3f} Msps")

    ax_spec.specgram(samples, NFFT=1024, Fs=args.rate / 1e6, Fc=args.freq / 1e6)
    ax_spec.set_xlabel("Time (s)")
    ax_spec.set_ylabel("Frequency (MHz)")
    ax_spec.set_title("Spectrogram")

    fig.tight_layout()
    out = "psd_capture.png"
    fig.savefig(out, dpi=150)
    print(f"saved {out} ({len(samples)} samples)")


if __name__ == "__main__":
    main()
