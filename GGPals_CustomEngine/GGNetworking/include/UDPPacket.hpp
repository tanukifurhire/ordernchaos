/*************************************************************************
  \file       UDPPacket.hpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
    Transport-layer packet structure with reliability metadata
  \details
    Defines Packet struct and PacketHeader for UDP reliability protocol.
    Handles low-level packet concerns: sequencing, checksums, type dispatch
    (CONNECT, ACK, DATA, FRAGMENT, HEARTBEAT, DISCONNECT), retransmission
    tracking, and acknowledgment state.

    PacketHeader is #pragma pack(1) aligned for predictable wire format.
    Includes RFC 1071 Internet Checksum for header integrity verification.
    Payload field contains serialized NetworkMessage for application data.

    Used by ReliabilityLayer for send/receive operations in custom reliability
    implementation over unreliable UDP transport.
  \dependencies
    UDPTypes.hpp, <vector>, <chrono>
  \see
    Packet, PacketHeader, PacketType, UDPPacket.cpp
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <UDPTypes.hpp>

#include <vector>
#include <chrono>

namespace GGNetworking
{
#pragma pack(push, 1)

    /*************************************************************************
      \brief
        UDP packet header for reliability layer
      \details
        Fixed-size header for all UDP packets. Handles sequencing, acknowledgment,
        and reliability metadata. This is the transport layer, not the game layer.

        PacketType is 1 Byte hence placed at the back
    *************************************************************************/
    struct PacketHeader
    {
        SequenceNum         sequenceNum{};
        uint16_t            payloadSize{};
        uint16_t            checksum{};       // Simple integrity check
        PacketType          packetType{};

        static constexpr size_t HEADER_SIZE = 9;
    };

    static_assert(PacketHeader::HEADER_SIZE == sizeof(PacketHeader), "HEADER_SIZE does not match struct size!");

#pragma pack(pop)

    /*************************************************************************
      \struct Packet
      \brief
        Transport-layer packet with header, payload, and transmission metadata.
      \details
        Represents a single UDP packet in reliability protocol. Contains serialization
        methods, retransmission tracking, and acknowledgment state. Used by
        ReliabilityLayer for send/receive operations.
      \see
        PacketHeader, ReliabilityLayer
    *************************************************************************/
    struct Packet
    {
    public:
        enum class PacketDeserializeStatus
        {
            BYTES_NOT_SAME_TOTAL = 502,
            INVALID_HEADER = 503,
            NO_PAYLOAD = 504,
            PACKET_DESERIALIZE_SUCCESS = 505
        };

        PacketHeader                header{};
        std::vector<uint8_t>        payload;
        std::chrono::milliseconds   timeSent{};
        uint32_t                    retransmissionCount{};
        bool                        acknowledged{ false };
        
        /*************************************************************************
          \brief
            Computes Internet checksum over packet header for integrity verification.
          \details
            Implements RFC 1071 Internet Checksum algorithm:
            1. Zeroes header.checksum field
            2. Treats header as array of 16-bit words
            3. Sums first 3 words (6 bytes) and last byte (skipping checksum field)
            4. Performs one's complement carry folding
            5. Stores bitwise NOT of result in header.checksum

            PacketHeader layout (9 bytes):
            - Bytes 0-5: sequenceNum (4) + payloadSize (2) -> summed
            - Bytes 6-7: checksum field -> excluded from calculation
            - Byte 8: packetType (1) -> summed
          \note
            Checksum covers both header and payload for full packet integrity.
            Called before Serialize() to embed checksum in outgoing packet.
          \warning
            Must be called after setting all other header fields. Modifying header
            after CalculateChecksum invalidates the checksum.
          \see
            ValidateHeader, Serialize, RFC 1071
        *************************************************************************/
        void CalculateChecksum();

        /*************************************************************************
          \brief
            Serializes packet to wire format byte array.
          \return
            Byte vector containing header + payload for sendto().
          \details
            Packs PacketHeader fields in little-endian, appends payload. Computes
            checksum over payload. Total size = HEADER_SIZE + payload.size().
        *************************************************************************/
        std::vector<uint8_t> Serialize() const;
        /*************************************************************************
          \brief
            Deserializes packet from received UDP datagram.
          \param[in] data
            Raw bytes from recvfrom().
          \param[in] size
            Byte count received.
          \return
            Status code indicating success or failure reason.
          \details
            Validates header fields (packetType, payloadSize, checksum), extracts
            payload. Returns error codes for malformed packets.
          \see
            PacketDeserializeStatus
        *************************************************************************/
        PacketDeserializeStatus Deserialize(uint8_t const* data, size_t size);
    private:
        /*************************************************************************
          \brief
            Validates packet header fields for corruption detection.
          \param[in] header
            Header to validate.
          \return
            true if header fields are within valid ranges.
          \details
            Checks: packetType < MAX_PACKET_TYPES, payloadSize <= MAX_PACKET_SIZE,
            fragmentIndex < fragmentTotal (if fragmented).
        *************************************************************************/
        bool ValidateHeader(PacketHeader const& header, uint8_t const* payloadData, size_t payloadLen);
        /*************************************************************************
          \brief
            Processes validated packet data into member fields.
          \param[in] data
            Raw packet bytes.
          \param[in] size
            Byte count.
          \details
            Called after successful validation. Copies header and payload into
            member variables.
        *************************************************************************/
        void PopulateIncomingPayload(uint8_t const* data, size_t size);
    };
}