#include "rtltcp.hpp"
#include "psd.hpp"

#include <getopt.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>

struct Args {
    std::string host = "127.0.0.1";
    int port = 1234;
    double freq = 100e6;
    double rate = 2.4e6;
    int gain = -1;       // -1 = leave on AGC
    size_t n = 1'048'576;
    std::string out = "iq.dat";
    std::string psdOut = "psd.dat";
    size_t nfft = 0;  // 0 = single periodogram over the whole capture; >0 = Welch's method
};

Args parseArgs(int argc, char** argv) {
    Args a;
    static struct option long_opts[] = {
        {"host",     required_argument, nullptr, 'h'},
        {"port",     required_argument, nullptr, 'p'},
        {"freq",     required_argument, nullptr, 'f'},
        {"rate",     required_argument, nullptr, 'r'},
        {"gain",     required_argument, nullptr, 'g'},
        {"n",        required_argument, nullptr, 'n'},
        {"out",      required_argument, nullptr, 'o'},
        {"psd-out",  required_argument, nullptr, 's'},
        {"nfft",     required_argument, nullptr, 'N'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'h': a.host = optarg; break;
            case 'p': a.port = std::atoi(optarg); break;
            case 'f': a.freq = std::atof(optarg); break;
            case 'r': a.rate = std::atof(optarg); break;
            case 'g': a.gain = std::atoi(optarg); break;
            case 'n': a.n = std::stoull(optarg); break;
            case 'o': a.out = optarg; break;
            case 's': a.psdOut = optarg; break;
            case 'N': a.nfft = std::stoull(optarg); break;
            default:
                std::cerr << "usage: " << argv[0]
                          << " [--host H] [--port P] [--freq Hz] [--rate Hz] [--gain tenthsDb]"
                             " [--n samples] [--out file] [--psd-out file] [--nfft segmentSize]\n";
                std::exit(1);
        }
    }
    return a;
}

int main(int argc, char** argv)
{
    Args args = parseArgs(argc, argv);
    RtlTcpClient client(args.host, args.port);
    client.setCenterFreq(args.freq);
    client.setSampleRate(args.rate);

    // drop the first chunk - tuner needs a moment to settle after retuning
    client.readSamples(static_cast<size_t>(args.rate * 0.2));
    auto iq = client.readSamples(args.n);

    // raw interleaved float32 I/Q
    std::ofstream out(args.out, std::ios::binary);
    out.write(reinterpret_cast<const char*>(iq.data()), iq.size() * sizeof(std::complex<float>));
    std::cout << "wrote " << iq.size() << " samples to " << args.out << "\n";

    // compute PSD (freq_hz, power_db columns)
    PSDResult psd = computePSD(iq, args.rate, args.freq, args.nfft);
    std::ofstream psdOut(args.psdOut);
    for (size_t i = 0; i < psd.freqsHz.size(); ++i)
        psdOut << psd.freqsHz[i] << " " << psd.powerDb[i] << "\n";
    std::cout << "wrote PSD to " << args.psdOut << "\n";

    return 0;
}