#ifndef MULTIPLEX_PACKET_H
#define MULTIPLEX_PACKET_H

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/multiplayer_peer.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include <cstdint>

enum MultiplexPacketSubtype : uint8_t {
	MUX_DATA = 0x00,
	MUX_CMD = 0x01
};

enum MultiplexPacketCommandSubtype : uint8_t {
	MUX_CMD_ADD_PEER = 0x00, // Sent from client->server to request a new peer, from server->client to inform old peers of new peers
	MUX_CMD_ADD_PEER_ACK = 0x01, // Sent in response to ADD_PEER, from server->client informing user that peer was created successfully
	MUX_CMD_ERR_SUBPEERS_EXCEEDED = 0x02, // Sent in response to ADD_PEER, from server->client, when adding a peer is rejected.
	MUX_CMD_ERR_SUBPEER_ID_EXISTS = 0x03, // Sent in response to ADD_PEER, from server->client, when adding a peer is rejected.
	MUX_CMD_REMOVE_PEER = 0x04, // Sent from server<->client to inform peer has left, always forced either direction, not necessarily sent on leave
};

// Commands are from MultiplexNetwork to MultiplexNetwork, routed through MultiplayerPeer
// i.e. MuxNet -> SteamMultiplayerPeer -> Steam Relay -> SteamMultiplayerPeer -> MuxNet
// These should not find themeselves in a MultiplexPeer
struct MultiplexPacketCommand {
	MultiplexPacketCommandSubtype subtype;
	int32_t subject_multiplex_peer;
};

// Data are from MultiplexPeer to MultiplexPeer, routed through Multiplex Network
// i.e. MuxPeer -> MuxNet -> SteamMultiplayerPeer -> Steam Relay -> SteamMultiplayerPeer -> MuxNet -> MuxPeer
struct MultiplexPacketData {
	uint32_t length;
	int32_t mux_peer_source;
	int32_t mux_peer_dest;
	uint8_t *data;
};

/*
 * serializes to
 *  0-0               uint8_t subtype = 0x00
 *  1-1               uint8_t transfer_mode 
 *  2-5               uint32_t length;
 *  6-9               int32_t mux_peer_source;
 *  10-13             int32_t mux_peer_dest;
 *  14-14+(len*8 - 1) uint8_t[length] data;
 * 
 * size is 14 + length
 *
 * OR
 *
 *  0-0 uint8_t subtype = 0x01
 *  1-1 uint8_t transfer_mode
 *  2-2 uint8_t command_subtype
 *  3-7 int32_t subject_multiplex_peer;
 *  size is 7
 *
 *  int32_t ARE CONVERTED TO BIG ENDIAN WHEN SERIALIZED, AKA NETWORK ORDER.
 */

class MultiplexPacket : public godot::RefCounted {
  GDCLASS(MultiplexPacket, godot::RefCounted);
public:
	MultiplexPacketSubtype subtype;
	godot::MultiplayerPeer::TransferMode transfer_mode;
	union {
		MultiplexPacketCommand command;
		MultiplexPacketData data;
	} contents;

  ~MultiplexPacket();
	// converts a multiplex packet into a newly allocated byte buffer in network order, returns length
  godot::PackedByteArray serialize();
	// converts a byte buffer into a multiplex packet, returns success or error
	godot::Error deserialize(godot::PackedByteArray& rawData);
  static void _bind_methods();
};
#endif
