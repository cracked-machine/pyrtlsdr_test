"""Minimal client for the rtl_tcp protocol.

Connects to a running `rtl_tcp` server and streams raw IQ samples as
numpy complex64 arrays. No librtlsdr bindings required - just the
socket protocol rtl_tcp speaks.

Protocol reference: rtl_tcp.c in the rtl-sdr project.
"""

import socket
import struct

import numpy as np

_CMD_SET_FREQ = 0x01
_CMD_SET_SAMPLE_RATE = 0x02
_CMD_SET_GAIN_MODE = 0x03
_CMD_SET_GAIN = 0x04
_CMD_SET_FREQ_CORRECTION = 0x05
_CMD_SET_AGC_MODE = 0x08
_CMD_SET_DIRECT_SAMPLING = 0x09
_CMD_SET_OFFSET_TUNING = 0x0A
_CMD_SET_BIAS_TEE = 0x0E


class RtlTcpClient:
    def __init__(self, host="127.0.0.1", port=1234, timeout=5.0):
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(None)
        header = self._recv_exact(12)
        magic, tuner_type, gain_count = struct.unpack(">4sII", header)
        if magic != b"RTL0":
            raise RuntimeError(f"unexpected magic bytes from server: {magic!r}")
        self.tuner_type = tuner_type
        self.tuner_gain_count = gain_count

    def _recv_exact(self, n):
        buf = bytearray(n)
        view = memoryview(buf)
        got = 0
        while got < n:
            chunk = self._sock.recv_into(view[got:])
            if chunk == 0:
                raise ConnectionError("rtl_tcp server closed the connection")
            got += chunk
        return bytes(buf)

    def _send_command(self, cmd, param):
        self._sock.sendall(struct.pack(">BI", cmd, param & 0xFFFFFFFF))

    def set_center_freq(self, freq_hz):
        self._send_command(_CMD_SET_FREQ, int(freq_hz))

    def set_sample_rate(self, rate_hz):
        self._send_command(_CMD_SET_SAMPLE_RATE, int(rate_hz))

    def set_gain_mode(self, manual):
        self._send_command(_CMD_SET_GAIN_MODE, 1 if manual else 0)

    def set_gain(self, gain_tenths_db):
        """gain_tenths_db: gain in tenths of a dB, e.g. 496 for 49.6 dB."""
        self._send_command(_CMD_SET_GAIN, int(gain_tenths_db))

    def set_freq_correction(self, ppm):
        self._send_command(_CMD_SET_FREQ_CORRECTION, int(ppm))

    def set_agc_mode(self, enabled):
        self._send_command(_CMD_SET_AGC_MODE, 1 if enabled else 0)

    def set_direct_sampling(self, mode):
        """mode: 0=off, 1=I-branch, 2=Q-branch."""
        self._send_command(_CMD_SET_DIRECT_SAMPLING, mode)

    def set_offset_tuning(self, enabled):
        self._send_command(_CMD_SET_OFFSET_TUNING, 1 if enabled else 0)

    def set_bias_tee(self, enabled):
        self._send_command(_CMD_SET_BIAS_TEE, 1 if enabled else 0)

    def read_samples(self, num_samples):
        """Read num_samples IQ samples, returned as complex64 in [-1, 1)."""
        raw = self._recv_exact(num_samples * 2)
        iq = np.frombuffer(raw, dtype=np.uint8).astype(np.float32)
        iq = (iq - 127.5) / 127.5
        return iq[0::2] + 1j * iq[1::2]

    def close(self):
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
