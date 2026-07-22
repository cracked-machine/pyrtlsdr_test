"""Connect to a local rtl_tcp server, demodulate a WBFM broadcast station, save audio.

Usage:
    rtl_tcp &
    python examples/fm_demod/python/main.py --freq 100.4e6 --seconds 10

Play back the result:
    aplay -f FLOAT_LE -r 44100 -c 1 audio.raw
"""

import argparse

import numpy as np
from scipy import signal

from examples.common.python.rtltcp import RtlTcpClient

# Mono WBFM broadcast: +-75 kHz deviation + 15 kHz audio bandwidth. Carson's
# rule puts occupied bandwidth at ~180 kHz, so we filter/decimate down to
# something comfortably wider than that rather than all the way to audio rate.
FM_CHANNEL_HALF_BW = 100e3
MAX_DEVIATION = 75e3


def fm_demod(iq, sample_rate):
    """Quadrature discriminator: phase difference between consecutive samples,
    normalized so full-scale deviation maps to +-1."""
    phase_diff = np.angle(iq[1:] * np.conj(iq[:-1]))
    return (phase_diff * sample_rate / (2 * np.pi * MAX_DEVIATION)).astype(np.float32)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=1234)
    p.add_argument("--freq", type=float, default=100.4e6, help="FM station frequency in Hz")
    p.add_argument("--rate", type=float, default=2.4e6, help="rtl_tcp capture sample rate in Hz")
    p.add_argument("--offset", type=float, default=250e3,
                    help="tune this far below --freq and mix back up digitally, "
                         "to keep the station off the rtl_tcp DC spike")
    p.add_argument("--decim", type=int, default=10, help="decimation factor applied after the channel filter")
    p.add_argument("--gain", type=float, default=None, help="tuner gain in dB (omit for AGC)")
    p.add_argument("--seconds", type=float, default=10.0, help="capture length in seconds")
    p.add_argument("--audio-rate", type=int, default=44100, help="output audio sample rate in Hz")
    p.add_argument("--out", default="audio.raw", help="output file for demodulated audio (raw float32 PCM)")
    args = p.parse_args()

    with RtlTcpClient(host=args.host, port=args.port) as sdr:
        sdr.set_sample_rate(args.rate)
        sdr.set_center_freq(args.freq - args.offset)
        if args.gain is None:
            sdr.set_gain_mode(False)
            sdr.set_agc_mode(True)
        else:
            sdr.set_gain_mode(True)
            sdr.set_gain(int(args.gain * 10))

        # drop the first chunk - tuner needs a moment to settle after retuning
        sdr.read_samples(int(args.rate * 0.2))
        iq = sdr.read_samples(int(args.rate * args.seconds))

    # 2. the station is sitting at +offset Hz because we tuned below it - mix it down to baseband
    t = np.arange(len(iq)) / args.rate
    iq = iq * np.exp(-1j * 2 * np.pi * args.offset * t)

    # 3. low-pass filter to isolate the FM channel and reject everything else
    taps = signal.firwin(127, FM_CHANNEL_HALF_BW, fs=args.rate)
    iq = signal.lfilter(taps, 1.0, iq)

    # 4. decimate down to a sane rate for demodulation
    iq = iq[:: args.decim]
    demod_rate = args.rate / args.decim

    # 5. FM demodulate
    audio = fm_demod(iq, demod_rate)

    # resample from the demod rate down to a standard audio rate (resample_poly
    # picks its own anti-aliasing filter for the, generally non-integer, ratio)
    audio = signal.resample_poly(audio, args.audio_rate, round(demod_rate)).astype(np.float32)

    # 6. save as raw float32 PCM so it can be played back
    audio.tofile(args.out)
    print(f"wrote {len(audio)} samples ({len(audio) / args.audio_rate:.1f}s) to {args.out} at {args.audio_rate} Hz")
    print(f"play with: aplay -f FLOAT_LE -r {args.audio_rate} -c 1 {args.out}")
    print(f"open raw with audacity: Encoding 32bit float, Little Endian, 1Channel, Fs={args.audio_rate}")

if __name__ == "__main__":
    main()
