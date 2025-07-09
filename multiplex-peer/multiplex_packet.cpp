#include "multiplex_packet.h"
#include <cstdio>
#include <endian.h>
#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/multiplayer_peer.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "multiplex_peer.h"

using namespace godot;


MultiplexPacket::~MultiplexPacket() {
  if (subtype == MUX_DATA) {
    delete contents.data.data;
  }
}


PackedByteArray MultiplexPacket::serialize() {
  PackedByteArray out;
  if (subtype == MUX_DATA) {
    out.resize(14 + sizeof(uint8_t) * contents.data.length);
    out.fill(0);
    out.encode_u8(0, (uint8_t)subtype);
    out.encode_u8(1, (uint8_t)transfer_mode);
    out.encode_u32(2, htobe32(contents.data.length));
    out.encode_s32(6, htobe32(contents.data.mux_peer_source));
    out.encode_s32(10,htobe32(contents.data.mux_peer_dest));
    for (int i = 0; i < contents.data.length; i++) {
      out.encode_u8(i + 14, contents.data.data[i]);
    }
  }
  else {
    out.resize(7);
    out.fill(0);
    out.encode_u8 (0, (uint8_t)subtype);
    out.encode_u8 (1, (uint8_t)transfer_mode);
    out.encode_u8 (2, (uint8_t)contents.command.subtype);
    out.encode_s32(3, htobe32((int32_t)contents.command.subject_multiplex_peer));
  }
  return out;
}

Error MultiplexPacket::deserialize(PackedByteArray& rawData) {
  subtype = (MultiplexPacketSubtype)(uint8_t)rawData.decode_u8(0);
  transfer_mode = (MultiplayerPeer::TransferMode)(uint8_t)rawData.decode_u8(1);
  switch (subtype) {
    case MUX_DATA:
      contents.data.length          = (uint32_t)be32toh(rawData.decode_u32(2 ));
      contents.data.mux_peer_source = ( int32_t)be32toh(rawData.decode_s32(6 ));
      contents.data.mux_peer_dest   = ( int32_t)be32toh(rawData.decode_s32(10));
      // Length and packet size mismatch could imply someone is trying to do a buffer overrun attack
      if (contents.data.length > rawData.size()-14) {
        printf("%d > %d", contents.data.length, (int)rawData.size() - 14);
      }
      ERR_FAIL_COND_V_MSG(contents.data.length > rawData.size() - 14, Error::ERR_INVALID_PARAMETER, "Packet reported length longer than packet received.");
      ERR_FAIL_COND_V_MSG(contents.data.length > MAX_MULTIPLEX_PACKET_SIZE - 14, Error::ERR_INVALID_PARAMETER, "Multiplex Packet too big to deserialize!");
      contents.data.data = new uint8_t[contents.data.length];
      for (int i = 0; i < contents.data.length; i++) {
        contents.data.data[i] = rawData.decode_u8(i + 14);
      }
      break;
    case MUX_CMD:
      contents.command.subtype = (MultiplexPacketCommandSubtype)(uint8_t)rawData.decode_u8(2);
      switch (contents.command.subtype) {
        case MUX_CMD_ADD_PEER:
        case MUX_CMD_ADD_PEER_ACK:
        case MUX_CMD_ERR_SUBPEERS_EXCEEDED:
        case MUX_CMD_ERR_SUBPEER_ID_EXISTS:
        case MUX_CMD_REMOVE_PEER:
          break;
        default:
          ERR_FAIL_V_MSG(godot::ERR_PARSE_ERROR, "Invalid multiplex command subtype, must be 0x00 to 0x04. Is the packet corrupted?");
      }
      contents.command.subject_multiplex_peer = (int32_t)be32toh(rawData.decode_s32(3));
      break;
    default:
      ERR_FAIL_V_MSG(godot::ERR_PARSE_ERROR, "Invalid multiplex packet subtype, must be 0x00 (DATA) or 0x01 (CMD)");
  }
  return OK;
}

void MultiplexPacket::_bind_methods() {
}
