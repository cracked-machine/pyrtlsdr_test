
This receives IQ data streamed from an RT-SDR dongle over TCP (using rtl-tcp) and plots a PSD and Spectrograph. Implemented in both python and C++. FFT is calculated using Numpy and KissFFT respectively.

You should run `scripts/restart_rtl_server.sh` on the server that has the USB dongle first.

Note: `--nfft N` enables Welch's method. the capture is split into overlapping N-sample segments (50% overlap), each Hann-windowed and FFT'd, and the power spectra are averaged before converting to dB. More segments equals less noisy noise floor, at the cost of frequency resolution (bin width becomes sampleRate/N instead of sampleRate/n).

 - `Fc=100MHz` 
 - `Fs=2.4GHz`

![](plot.png)