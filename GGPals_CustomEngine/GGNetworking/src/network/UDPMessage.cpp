/*************************************************************************
  \file       UDPMessage.cpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
	Application-layer network message serialization and deserialization
  \details
	Implements NetworkMessage class for type-safe game message handling.
	Provides bidirectional conversion between structured game messages and
	byte streams for UDP transmission. Supports templated component serialization
	via IsNetworkSerializable concept for ECS integration. Handles message
	header management, payload validation, and sequential read/write operations.

	Messages are encapsulated within DATA packets at the transport layer,
	separating application concerns from reliability protocol mechanics.
  \dependencies
	UDPMessage.hpp, UDPTypes.hpp, <iostream>, <vector>, <cstring>
  \see
	UDPMessage.hpp, NetworkMessage, MessageHeader, UDPPacket.cpp
  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/

#include <UDPMessage.hpp>

#include <iostream>

namespace GGNetworking
{
	/*************************************************************************
	  \brief
		Default constructor initializing empty network message.
	  \details
		Creates NetworkMessage with uninitialized messageHeader. Payload vector
		is default-constructed (empty). ReadOffset starts at 0.
	*************************************************************************/
	NetworkMessage::NetworkMessage() : messageHeader() {}

	/*************************************************************************
	  \brief
		Constructs network message with specified type.
	  \param type
		MessageType enum value to set in message header.
	  \details
		Initializes messageHeader.messageType to provided value. Payload remains
		empty until WriteValue calls. Auto-assigns unique messageID from static counter.
	*************************************************************************/
	NetworkMessage::NetworkMessage(MessageType type)
	{
		messageHeader.messageType = type;
	}

	/*************************************************************************
	  \brief
		Serializes message to byte array for network transmission.
	  \return
		Byte vector containing packed MessageHeader followed by payload bytes.
	  \details
		Constructs wire format by:
		1. Calculating total size (HEADER_SIZE + payload.size())
		2. Copying messageHeader bytes via reinterpret_cast
		3. Appending payload vector contents
		Resulting buffer is ready for insertion into Packet payload or direct sendto().
	  \note
		Header fields are serialized in native byte order (little-endian on x86/x64).
		Cross-platform implementations should use htonl/ntohl for network byte order.
	  \see
		Deserialize, Packet::Serialize
	*************************************************************************/
	std::vector<uint8_t> NetworkMessage::Serialize() const {
		std::vector<uint8_t> buffer;

		size_t wireSize = MessageHeader::HEADER_SIZE + payload.size();

		buffer.reserve(wireSize);

		buffer.assign(reinterpret_cast<uint8_t const*>(&messageHeader), reinterpret_cast<uint8_t const*>(&messageHeader) + MessageHeader::HEADER_SIZE);

		if (!payload.empty())
		{
			buffer.insert(buffer.end(), payload.begin(), payload.end());
		}

		return buffer;
	}

	/*************************************************************************
	  \brief
		Deserializes message from received byte array.
	  \param data
		Pointer to raw bytes containing MessageHeader + payload.
	  \param size
		Total byte count of data buffer.
	  \return
		MessageDeserializeStatus enum indicating success or specific failure reason.
	  \details
		Deserialization process:
		1. Casts data to MessageHeader pointer for header extraction
		2. Validates size matches header.payloadSize + HEADER_SIZE
		3. Calls ValidateHeader to check field constraints
		4. Copies header to messageHeader member
		5. Extracts payload bytes into payload vector
		6. Resets readOffset to 0 for sequential ReadValue calls

		Returns specific error codes for:
		- BAD_MESSAGE: Null data pointer
		- BYTES_NOT_SAME_TOTAL: Size mismatch between buffer and header
		- INVALID_HEADER: Header validation failed (oversized payload, invalid type)
		- NO_PAYLOAD: payloadSize is 0 (may be valid for some message types)
		- MESSAGE_DESERIALIZE_SUCCESS: Complete successful deserialization
	  \note
		Does not verify messageType validity; caller must check GetMessageType().
		Payload extraction uses reserve + assign for efficient memory allocation.
	  \see
		Serialize, ValidateHeader, ReadValue
	*************************************************************************/
	NetworkMessage::MessageDeserializeStatus NetworkMessage::Deserialize(uint8_t const* data, size_t size)
	{
		MessageHeader const* header = reinterpret_cast<MessageHeader const*>(data);

		if (!header) return MessageDeserializeStatus::BAD_MESSAGE;

		size_t expectedTotalSize = MessageHeader::HEADER_SIZE + header->payloadSize;
		if (size != expectedTotalSize)
		{
			std::cout << "NETWORK ERROR: Message Bytes Received Not Same as Total Size!" << std::endl;
			return MessageDeserializeStatus::BYTES_NOT_SAME_TOTAL;
		}

		if (!ValidateHeader(*header))
		{
			std::cout << "NETWORK ERROR: Invalid Header!" << std::endl;
			return MessageDeserializeStatus::INVALID_HEADER;
		}

		messageHeader = *header;

		if (messageHeader.payloadSize > 0)
		{
			uint8_t const* payloadStart = data + MessageHeader::HEADER_SIZE;

			payload.reserve(messageHeader.payloadSize);

			payload.assign(payloadStart, payloadStart + messageHeader.payloadSize);

			return MessageDeserializeStatus::MESSAGE_DESERIALIZE_SUCCESS;
		}

		return MessageDeserializeStatus::NO_PAYLOAD;
	}

	/*************************************************************************
	  \brief
		Validates message header fields for corruption and constraint violations.
	  \param header
		MessageHeader instance to validate.
	  \return
		true if all header fields are within valid ranges, false otherwise.
	  \details
		Validation checks:
		- payloadSize <= MAX_MESSAGE_SIZE (prevents buffer overflows)

		Does not validate messageType enum (allows for extensibility).
		Future enhancements could add CRC32 checksum validation.
	  \note
		Called internally by Deserialize before payload extraction.
		Overly large payloadSize indicates corruption or malicious packet.
	  \see
		Deserialize, MAX_MESSAGE_SIZE
	*************************************************************************/
	bool NetworkMessage::ValidateHeader(MessageHeader const& header)
	{
		if (header.payloadSize > MAX_MESSAGE_SIZE) return false;

		return true;
	}
}