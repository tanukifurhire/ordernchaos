/*************************************************************************
  \file       UDPPacket.cpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
	Transport-layer UDP packet handling with checksum validation
  \details
	Implements Packet struct for low-level UDP datagram management in custom
	reliability protocol. Provides RFC 1071 Internet Checksum computation for
	header integrity verification, binary serialization for wire format, and
	comprehensive validation for incoming packets. Handles packet type dispatch
	(CONNECT, ACK, DATA, FRAGMENT, HEARTBEAT, DISCONNECT) with detailed logging.

	Separates transport concerns (sequencing, checksums, packet types) from
	application-layer message content (handled by NetworkMessage).
  \dependencies
	UDPPacket.hpp, UDPTypes.hpp, <iostream>, <vector>
  \see
	UDPPacket.hpp, Packet, PacketHeader, UDPReliabilityLayer.cpp
  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <UDPPacket.hpp>

#include <iostream>

namespace GGNetworking
{

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
	void Packet::CalculateChecksum()
	{
		PacketHeader pHeader = header;
		pHeader.checksum = 0;

		// Sum header bytes (first 6 bytes + last 1 byte, skipping checksum field)
		uint16_t const* words = reinterpret_cast<uint16_t const*>(&pHeader);
		uint32_t sum{};

		for (size_t i{}; i < 3; i++)
		{
			sum += words[i];
		}

		sum += reinterpret_cast<uint8_t const*>(&pHeader)[PacketHeader::HEADER_SIZE - 1];

		// Sum payload bytes as 16-bit words
		size_t payloadLen = payload.size();
		uint16_t const* payloadWords = reinterpret_cast<uint16_t const*>(payload.data());
		size_t fullWords = payloadLen / 2;

		for (size_t i{}; i < fullWords; i++)
		{
			sum += payloadWords[i];
		}

		// Handle odd trailing byte
		if (payloadLen % 2 != 0)
		{
			sum += payload[payloadLen - 1];
		}

		// One's complement carry folding
		while (sum >> 16)
		{
			sum = (sum & 0xFFFF) + (sum >> 16);
		}

		uint16_t checksum = ~sum & 0xFFFF;

		// RFC 768: transmit 0xFFFF instead of 0x0000 (both represent zero
		// in one's complement, but 0 means "no checksum" in our protocol)
		if (checksum == 0) checksum = 0xFFFF;

		header.checksum = checksum;
	}

	/*************************************************************************
	  \brief
		Serializes packet to wire format byte array for UDP transmission.
	  \return
		Byte vector containing PacketHeader followed by payload bytes.
	  \details
		Constructs UDP datagram by:
		1. Calculating wire size (HEADER_SIZE + payload.size())
		2. Reserving buffer capacity to avoid reallocations
		3. Copying header bytes via reinterpret_cast
		4. Appending payload vector contents if non-empty

		Resulting buffer is ready for sendto() syscall.
	  \note
		Assumes header.checksum already computed via CalculateChecksum().
		Header fields are serialized in native byte order (little-endian on x86/x64).
	  \warning
		Does not validate header fields. Caller must ensure header is properly
		initialized (packetType set, payloadSize matches payload.size()).
	  \see
		CalculateChecksum, Deserialize, ReliabilityLayer::SendPacket
	*************************************************************************/
	std::vector<uint8_t> Packet::Serialize() const
	{
		std::vector<uint8_t> retVal{};

		size_t const wireSize = PacketHeader::HEADER_SIZE + payload.size();

		retVal.reserve(wireSize);

		retVal.assign(reinterpret_cast<uint8_t const*>(&header), reinterpret_cast<uint8_t const*>(&header) + PacketHeader::HEADER_SIZE);

		if (!payload.empty())
		{
			retVal.insert(retVal.end(), payload.begin(), payload.end());
		}

		return retVal;
	}

	/*************************************************************************
	  \brief
		Deserializes packet from received UDP datagram.
	  \param data
		Pointer to raw bytes from recvfrom() syscall.
	  \param size
		Total byte count received in datagram.
	  \return
		PacketDeserializeStatus enum indicating success or specific failure reason.
	  \details
		Deserialization and validation process:
		1. Casts data to PacketHeader pointer for header extraction
		2. Validates size matches header.payloadSize + HEADER_SIZE
		3. Calls ValidateHeader for checksum and field validation
		4. Copies payload bytes via PopulateIncomingPayload
		5. Logs packet type via switch statement for debugging

		Returns specific error codes for:
		- BYTES_NOT_SAME_TOTAL: Size mismatch (packet truncation/corruption)
		- INVALID_HEADER: Validation failed (bad checksum, invalid type/size)
		- NO_PAYLOAD: payloadSize is 0 (ACK/HEARTBEAT packets)
		- PACKET_DESERIALIZE_SUCCESS: Complete successful deserialization
	  \note
		Logs detailed messages for each packet type (CONNECT, ACK, DATA, etc.).
		Debug output helps trace protocol state machine transitions.
	  \warning
		Does not perform replay attack detection. Caller must check sequenceNum
		against expected values in ClientConnection.
	  \see
		Serialize, ValidateHeader, PopulateIncomingPayload
	*************************************************************************/
	Packet::PacketDeserializeStatus Packet::Deserialize(uint8_t const* data, size_t size)
	{
		PacketHeader const* headerPacket = reinterpret_cast<PacketHeader const*>(data);

		size_t expectedTotalSize = PacketHeader::HEADER_SIZE + headerPacket->payloadSize;

		if (size != expectedTotalSize)
		{
			std::cout << "NETWORK ERROR: Packet " << size << " bytes Received Not Same as Total Size " << expectedTotalSize << "!" << std::endl;
			return PacketDeserializeStatus::BYTES_NOT_SAME_TOTAL;
		}

		// Pass payload pointer and size to validate checksum over header + payload
		uint8_t const* payloadStart = data + PacketHeader::HEADER_SIZE;
		size_t payloadLen = headerPacket->payloadSize;

		if (!ValidateHeader(*headerPacket, payloadStart, payloadLen))
		{
			std::cout << "NETWORK ERROR: Invalid Header!" << std::endl;
			return PacketDeserializeStatus::INVALID_HEADER;
		}

		// Always copy header (fixes ACK packets with payloadSize==0 being misidentified)
		header = *headerPacket;

		if (headerPacket->payloadSize > 0)
		{
			PopulateIncomingPayload(payloadStart, headerPacket->payloadSize);
		}

		switch (header.packetType)
		{
		case PacketType::CONNECT:
		{
			std::cout << "Connect request received!" << std::endl;
			break;
		}
		case PacketType::ACK:
		{
			//std::cout << "Acknowledgement request received!" << std::endl;
			break;
		}
		case PacketType::DATA:
		{
			//std::cout << "Data packet received!" << std::endl;
			break;
		}
		case PacketType::FRAGMENT:
		{
			std::cout << "[FRAG] Fragment packet received! payloadSize=" << header.payloadSize << std::endl;
			break;
		}
		case PacketType::HEARTBEAT:
		{
			//std::cout << "This packet is a heartbeat!" << std::endl;
			break;
		}
		case PacketType::DISCONNECT:
		{
			std::cout << "This is a Disconnect request!" << std::endl;
			break;
		}
		}

		return PacketDeserializeStatus::PACKET_DESERIALIZE_SUCCESS;
	}

	/*************************************************************************
	  \brief
		Validates packet header fields and verifies checksum integrity.
	  \param headerVal
		PacketHeader instance to validate (from incoming packet).
	  \return
		true if all validations pass, false on any failure.
	  \details
		Validation steps:
		1. Checks packetType < MAX_PACKET_TYPES (prevents undefined behavior)
		2. Checks payloadSize <= MAX_PACKET_SIZE (prevents buffer overflows)
		3. Recomputes checksum using same algorithm as CalculateChecksum()
		4. Compares computed checksum with headerVal.checksum

		Checksum validation:
		- Zeroes checksum field in temporary header copy
		- Sums header bytes as 16-bit words (excluding checksum field)
		- Performs one's complement arithmetic with carry folding
		- Inverts result and compares with received checksum

		Logs detailed error messages:
		- "Invalid Packet Checksum!" for checksum mismatch
		- "No Checksum!" if checksum field is zero (protocol violation)
	  \note
		Called internally by Deserialize before payload extraction.
		Checksum validation is mandatory; packets without checksums are rejected.
	  \warning
		Does not validate sequenceNum ordering. Caller must implement sequence
		tracking in ClientConnection for duplicate/out-of-order detection.
	  \see
		CalculateChecksum, Deserialize, RFC 1071
	*************************************************************************/
	bool Packet::ValidateHeader(PacketHeader const& headerVal, uint8_t const* payloadData, size_t payloadLen)
	{
		uint8_t packetType = static_cast<uint8_t>(headerVal.packetType);
		if (packetType > static_cast<uint8_t>(PacketType::MAX_PACKET_TYPES)) return false;
		if (headerVal.payloadSize > MAX_MESSAGE_SIZE + 20) return false;

		if (headerVal.checksum)
		{
			PacketHeader pHeader = headerVal;
			pHeader.checksum = 0;

			// Sum header bytes (first 6 bytes + last 1 byte, skipping checksum field)
			uint16_t const* words = reinterpret_cast<uint16_t const*>(&pHeader);
			uint32_t sum{};

			for (size_t i{}; i < 3; i++)
			{
				sum += words[i];
			}

			sum += reinterpret_cast<uint8_t const*>(&pHeader)[PacketHeader::HEADER_SIZE - 1];

			// Sum payload bytes as 16-bit words
			uint16_t const* payloadWords = reinterpret_cast<uint16_t const*>(payloadData);
			size_t fullWords = payloadLen / 2;

			for (size_t i{}; i < fullWords; i++)
			{
				sum += payloadWords[i];
			}

			// Handle odd trailing byte
			if (payloadLen % 2 != 0)
			{
				sum += payloadData[payloadLen - 1];
			}

			// One's complement carry folding
			while (sum >> 16)
			{
				sum = (sum & 0xFFFF) + (sum >> 16);
			}

			uint16_t checksum = ~sum & 0xFFFF;
			if (checksum == 0) checksum = 0xFFFF;

			if (checksum != headerVal.checksum)
			{
				std::cout << "Invalid Packet Checksum!" << std::endl;
				return false;
			}
		}
		else
		{
			std::cout << "No Checksum!" << std::endl;
			return false;
		}

		return true;
	}

	/*************************************************************************
	  \brief
		Extracts and stores payload bytes from deserialized packet.
	  \param data
		Pointer to payload start (data + HEADER_SIZE).
	  \param size
		Payload byte count (from header.payloadSize).
	  \details
		Payload extraction process:
		1. Clears existing payload vector
		2. Resizes vector to payloadSize for capacity
		3. Copies bytes via assign (efficient single allocation)

		Called internally by Deserialize after header validation passes.
	  \note
		Uses vector::assign instead of insert for better performance on large payloads.
		Payload vector is guaranteed to be exactly payloadSize bytes after this call.
	  \see
		Deserialize, std::vector::assign
	*************************************************************************/
	void Packet::PopulateIncomingPayload(uint8_t const* data, size_t size)
	{
		payload.clear();
		payload.resize(size);
		payload.assign(data, data + size);
	}
}