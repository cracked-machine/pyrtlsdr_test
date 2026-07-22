#include "rtltcp.hpp"

RtlTcpClient::RtlTcpClient(const std::string& host, int port) 
{
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(port);

    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0)
        throw std::runtime_error("getaddrinfo failed for " + host);

    sock_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_ < 0 || connect(sock_, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("could not connect to rtl_tcp at " + host + ":" + port_str);
    }
    freeaddrinfo(res);

    uint8_t header[12];
    recvExact(header, sizeof(header));

    char magic[5] = {0};
    std::memcpy(magic, header, 4);
    if (std::memcmp(magic, "RTL0", 4) != 0)
        throw std::runtime_error(std::string("unexpected magic bytes from server: ") + magic);

    uint32_t tuner_type_be, gain_count_be;
    std::memcpy(&tuner_type_be, header + 4, 4);
    std::memcpy(&gain_count_be, header + 8, 4);
    tuner_type_ = ntohl(tuner_type_be);
    tuner_gain_count_ = ntohl(gain_count_be);
}

std::vector<std::complex<float>> RtlTcpClient::readSamples(size_t numSamples) {
    // IQ is two interleaved bytes, so we need twice as much data.
    std::vector<uint8_t> raw(numSamples * 2);
    recvExact(raw.data(), raw.size());

    std::vector<std::complex<float>> iq(numSamples);
    for (size_t i = 0; i < numSamples; ++i) {
        float I = (static_cast<float>(raw[2 * i])     - 127.5f) / 127.5f;
        float Q = (static_cast<float>(raw[2 * i + 1]) - 127.5f) / 127.5f;
        iq[i] = {I, Q};
    }
    return iq;
}

void RtlTcpClient::recvExact(void* buf, size_t n) {
    auto* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < n) {
        ssize_t chunk = recv(sock_, p + got, n - got, 0);
        if (chunk <= 0)
            throw std::runtime_error("rtl_tcp server closed the connection");
        got += static_cast<size_t>(chunk);
    }
}

void RtlTcpClient::sendCommand(uint8_t cmd, uint32_t param) {
    uint8_t buf[5];
    buf[0] = cmd;
    uint32_t be_param = htonl(param);
    std::memcpy(buf + 1, &be_param, 4);
    if (send(sock_, buf, sizeof(buf), 0) != sizeof(buf))
        throw std::runtime_error("failed to send command to rtl_tcp");
}