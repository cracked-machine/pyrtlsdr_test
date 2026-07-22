// rtltcp.hpp
#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <complex>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// See https://github.com/osmocom/rtl-sdr/blob/master/src/rtl_tcp.c
namespace rtltcp_cmd {
constexpr uint8_t SET_FREQ            = 0x01;
constexpr uint8_t SET_SAMPLE_RATE     = 0x02;
constexpr uint8_t SET_GAIN_MODE       = 0x03;
constexpr uint8_t SET_GAIN            = 0x04;
constexpr uint8_t SET_FREQ_CORRECTION = 0x05;
constexpr uint8_t SET_AGC_MODE        = 0x08;
constexpr uint8_t SET_DIRECT_SAMPLING = 0x09;
constexpr uint8_t SET_OFFSET_TUNING   = 0x0A;
constexpr uint8_t SET_BIAS_TEE        = 0x0E;
}  // namespace rtltcp_cmd

class RtlTcpClient {
public:
    RtlTcpClient(const std::string& host = "127.0.0.1", int port = 1234);
    ~RtlTcpClient() { close(); }

    // non-copyable, movable would need extra plumbing - keep it simple/non-copyable for now
    RtlTcpClient(const RtlTcpClient&) = delete;
    RtlTcpClient& operator=(const RtlTcpClient&) = delete;

    void setCenterFreq(double freqHz) { sendCommand(rtltcp_cmd::SET_FREQ, static_cast<uint32_t>(freqHz)); }

    void setSampleRate(double rateHz) { sendCommand(rtltcp_cmd::SET_SAMPLE_RATE, static_cast<uint32_t>(rateHz)); }

    void setGainMode(bool manual) { sendCommand(rtltcp_cmd::SET_GAIN_MODE, manual ? 1 : 0); }

    // gainTenthsDb: gain in tenths of a dB, e.g. 496 for 49.6 dB.
    void setGain(int gainTenthsDb) { sendCommand(rtltcp_cmd::SET_GAIN, static_cast<uint32_t>(gainTenthsDb)); }

    void setFreqCorrection(int ppm) { sendCommand(rtltcp_cmd::SET_FREQ_CORRECTION, static_cast<uint32_t>(ppm)); }

    void setAgcMode(bool enabled) { sendCommand(rtltcp_cmd::SET_AGC_MODE, enabled ? 1 : 0); }

    // mode: 0=off, 1=I-branch, 2=Q-branch.
    void setDirectSampling(int mode) { sendCommand(rtltcp_cmd::SET_DIRECT_SAMPLING, static_cast<uint32_t>(mode)); }

    void setOffsetTuning(bool enabled) { sendCommand(rtltcp_cmd::SET_OFFSET_TUNING, enabled ? 1 : 0); }

    void setBiasTee(bool enabled) { sendCommand(rtltcp_cmd::SET_BIAS_TEE, enabled ? 1 : 0); }

    // Read numSamples IQ samples, returned as complex<float> in [-1, 1).
    std::vector<std::complex<float>> readSamples(size_t numSamples);

    void close() {
        if (sock_ >= 0) {
            ::close(sock_);
            sock_ = -1;
        }
    }

    uint32_t tunerType() const { return tuner_type_; }
    uint32_t tunerGainCount() const { return tuner_gain_count_; }

private:
    void recvExact(void* buf, size_t n);
    void sendCommand(uint8_t cmd, uint32_t param);
    int sock_ = -1;
    uint32_t tuner_type_ = 0;
    uint32_t tuner_gain_count_ = 0;
};
