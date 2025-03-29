
#include <vector>
#include <glm/vec3.hpp>
#include <WinSock2.h>

namespace NetworkUtils {

    enum DATA_TYPE {
        DT_SHORT,
        DT_LONG,
        DT_CHAR
    };

    template <typename T>
    void WriteToPacket(std::vector<char>& packet, const T& data, DATA_TYPE type) {
        T netData;
        if (type == DT_SHORT) netData = htons(data);
        else if (type == DT_LONG) netData = htonl(data);
        else netData = data;

        const char* rawData = reinterpret_cast<const char*>(&netData);
        packet.insert(packet.end(), rawData, rawData + sizeof(T));
    }

    uint32_t FloatToNetwork(float f) {
        union { float f; uint32_t i; } u;
        u.f = f;
        return u.i;
    }

    void WriteVec3(std::vector<char>& packet, const glm::vec3 vec) {
        WriteToPacket(packet, FloatToNetwork(vec.x), DATA_TYPE::DT_LONG);
        WriteToPacket(packet, FloatToNetwork(vec.y), DATA_TYPE::DT_LONG);
        WriteToPacket(packet, FloatToNetwork(vec.z), DATA_TYPE::DT_LONG);
    }

    // READING
    template <typename T>
    void ReadFromPacket(const char* packet, size_t offset, T& outData, DATA_TYPE type) {
        std::memcpy(&outData, &packet[offset], sizeof(T));

        if (type == DT_SHORT) {
            outData = ntohs(outData);
        } else if (type == DT_LONG) {
            outData = ntohl(outData);
        }
    }

    float NetworkToFloat(uint32_t i) {
        union { float f; uint32_t i; } u;
        u.i = i;
        return u.f;
    }

    void ReadVec3(const char* packet, size_t offset, glm::vec3& vec) {
        uint32_t tmp;
        NetworkUtils::ReadFromPacket(packet, offset, tmp, DATA_TYPE::DT_LONG);
        vec.x = NetworkToFloat(tmp);

        NetworkUtils::ReadFromPacket(packet, offset + 4, tmp, DATA_TYPE::DT_LONG);
        vec.y = NetworkToFloat(tmp);

        NetworkUtils::ReadFromPacket(packet, offset + 8, tmp, DATA_TYPE::DT_LONG);
        vec.z = NetworkToFloat(tmp);
    }
}