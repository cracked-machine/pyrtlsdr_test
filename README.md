
This receives IQ data streamed from an RT-SDR dongle over TCP (using rtl-tcp) and plots a PSD and Spectrograph. Implemented in both python and C++. FFT is calculated using Numpy and KissFFT respectively.

 - `Fc=100MHz` 
 - `Fs=2.4GHz`

![](plot.png)

## Quickstart

1. Clone this repo to both the server (the machine with the RTL SDR USB dongle) and client (can be the same machine)
1. Install `rtl-sdr`  on the server:

    ```
    sudo apt update
    sudo apt install rtl-sdr librtlsdr-dev
    ```

1. Disable the default DVB USB dongle driver:

    ```
    echo 'blacklist dvb_usb_rtl28xxu' | sudo tee /etc/modprobe.d/blacklist-dvb_usb_rtl28xxu.conf
    ```

1. Run `scripts/restart_rtl_server.sh` on the server. Check for errors.
1. Run `scripts/test_cpp.sh` or `scripts/test_python.sh` on the client.

----

Note: `--nfft N` enables Welch's method. the capture is split into overlapping N-sample segments (50% overlap), each Hann-windowed and FFT'd, and the power spectra are averaged before converting to dB. More segments equals less noisy noise floor, at the cost of frequency resolution (bin width becomes sampleRate/N instead of sampleRate/n).

