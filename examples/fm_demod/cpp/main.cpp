#include <rtltcp.hpp>
#include "fm_demod.hpp"

#include <getopt.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>

struct Args {
    std::string host = "127.0.0.1";
    int port = 1234;
    double freq = 100.4e6;    // FM station frequency in Hz
    double rate = 2.4e6;      // rtl_tcp capture sample rate in Hz
    double offset = 250e3;    // tune this far below freq and mix back up digitally,
                              // to keep the station off the rtl_tcp DC spike
    int decim = 10;           // decimation factor applied after the channel filter
    int gain = -1;            // tenths of a dB, -1 = leave on AGC
    double seconds = 10.0;    // capture length in seconds
    int audioRate = 44100;    // output audio sample rate in Hz
    std::string out = "audio.raw";
};

Args parseArgs(int argc, char** argv) {
    Args a;
    static struct option long_opts[] = {
        {"host",       required_argument, nullptr, 'h'},
        {"port",       required_argument, nullptr, 'p'},
        {"freq",       required_argument, nullptr, 'f'},
        {"rate",       required_argument, nullptr, 'r'},
        {"offset",     required_argument, nullptr, 'o'},
        {"decim",      required_argument, nullptr, 'd'},
        {"gain",       required_argument, nullptr, 'g'},
        {"seconds",    required_argument, nullptr, 's'},
        {"audio-rate", required_argument, nullptr, 'A'},
        {"out",        required_argument, nullptr, 'O'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'h': a.host = optarg; break;
            case 'p': a.port = std::atoi(optarg); break;
            case 'f': a.freq = std::atof(optarg); break;
            case 'r': a.rate = std::atof(optarg); break;
            case 'o': a.offset = std::atof(optarg); break;
            case 'd': a.decim = std::atoi(optarg); break;
            case 'g': a.gain = std::atoi(optarg); break;
            case 's': a.seconds = std::atof(optarg); break;
            case 'A': a.audioRate = std::atoi(optarg); break;
            case 'O': a.out = optarg; break;
            default:
                std::cerr << "usage: " << argv[0]
                          << " [--host H] [--port P] [--freq Hz] [--rate Hz] [--offset Hz]"
                             " [--decim N] [--gain tenthsDb] [--seconds S] [--audio-rate Hz] [--out file]\n";
                std::exit(1);
        }
    }
    return a;
}

int main(int argc, char** argv)
{
    Args args = parseArgs(argc, argv);

    RtlTcpClient client(args.host, args.port);
    client.setSampleRate(args.rate);
    client.setCenterFreq(args.freq - args.offset);
    if (args.gain < 0) {
        client.setGainMode(false);
        client.setAgcMode(true);
    } else {
        client.setGainMode(true);
        client.setGain(args.gain);
    }

    // drop the first chunk - tuner needs a moment to settle after retuning
    client.readSamples(static_cast<size_t>(args.rate * 0.2));
    auto iq = client.readSamples(static_cast<size_t>(args.rate * args.seconds));
    client.close();

    // 2. the station is sitting at +offset Hz because we tuned below it - mix it down to baseband
    for (size_t n = 0; n < iq.size(); ++n) {
        double phase = -2.0 * M_PI * args.offset * n / args.rate;
        iq[n] *= std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }

    // 3. low-pass filter to isolate the FM channel and reject everything else
    std::vector<float> taps = designLowpassFir(127, kFmChannelHalfBw, args.rate);
    iq = filterComplex(taps, iq);

    // 4. decimate down to a sane rate for demodulation
    std::vector<std::complex<float>> iqDecim;
    iqDecim.reserve(iq.size() / args.decim + 1);
    for (size_t n = 0; n < iq.size(); n += args.decim) iqDecim.push_back(iq[n]);
    double demodRate = args.rate / args.decim;

    // 5. FM demodulate
    std::vector<float> audio = fmDemodulate(iqDecim, demodRate);

    // resample from the demod rate down to a standard audio rate
    audio = resamplePoly(audio, args.audioRate, static_cast<int>(std::lround(demodRate)), demodRate);

    // 6. save as raw float32 PCM so it can be played back
    std::ofstream out(args.out, std::ios::binary);
    out.write(reinterpret_cast<const char*>(audio.data()), audio.size() * sizeof(float));
    std::cout << "wrote " << audio.size() << " samples (" << (audio.size() / static_cast<double>(args.audioRate))
              << "s) to " << args.out << " at " << args.audioRate << " Hz\n";
    std::cout << "play with: aplay -f FLOAT_LE -r " << args.audioRate << " -c 1 " << args.out << "\n";

    return 0;
}
