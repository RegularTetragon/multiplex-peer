#ifndef MULTIPLEX_PEER_H
#define MULTIPLEX_PEER_H

#include "godot_cpp/classes/multiplayer_peer.hpp"
#include "multiplex_network.h"
#include <cstdint>
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
#include <godot_cpp/templates/hash_map.hpp>
using namespace godot;
// size of a steam packet minus some overhead.
#define MAX_MULTIPLEX_PACKET_SIZE 522288
// MultiplexPeer wraps other Peers to allow for multiple virtual connections by decorating the packets
// with only one actual network peer in the tree
/*
* // For hosting with split screen multiplayer
* const server_peer = ENetMultiplayerPeer.new()
* server_peer.start_server(port)
*
* const server_multiplexing_peer = MultiplexPeer.new()
* server_multiplexing_peer.multiplex_server(server)
* server = packed_server.instantiate()
* server.set_multiplayer_peer(server_multiplexing_peer)
* for i in range(num_players):
*   const client_multiplexing_peer = server_multiplexing_peer.make_local_client()
*   client = packed_client.instantiate()
*   client.set_multiplayer_peer(client_multiplexing_peer


* // For joining with split screen multiplayer
* const client_peer = ENetMultiplayerPeer.new()
* client_peer.start_client(address, port)
*
* const client_multiplexing_peer = MultiplexPeer.new()
* client_multiplexing_peer.multiplex_client(client_peer)
* for i in range(num_players):
*   client = packed_client.instantiate()
*   client.set_multiplayer_peer(client_multiplexing_peer.make_local_client())
*/
class MultiplexPeer : public MultiplayerPeerExtension {
	GDCLASS(MultiplexPeer, MultiplayerPeerExtension)

private:
	enum Mode {
		MODE_NONE,
		MODE_SERVER,
		MODE_CLIENT
	};
	Mode active_mode = MODE_NONE;
	Ref<MultiplexNetwork> network;
	List<Ref<MultiplexPacket>> incoming_packets;
	Ref<MultiplexPacket> current_packet;
	int32_t unique_id = 0;
	int32_t target_peer = 0;
	int32_t current_channel = 0;
	int32_t max_players = 0;
	MultiplayerPeer::ConnectionStatus connection_status = CONNECTION_DISCONNECTED;
	MultiplayerPeer::TransferMode current_transfer_mode = TRANSFER_MODE_RELIABLE;

protected:
  static void _bind_methods();

public:
	Error _put_multiplex_packet_direct(Ref<MultiplexPacket> packet);
	Error create_server(Ref<MultiplexNetwork> network, int max_players);
	Error create_client(Ref<MultiplexNetwork> network);
  void complete_connection();
	MultiplexPeer();
	~MultiplexPeer();
	Error _get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size) override;
	Error _put_packet(const uint8_t *p_buffer, int32_t p_buffer_size) override;
	int32_t _get_available_packet_count() const override;
	int32_t _get_max_packet_size() const override;
	// PackedByteArray _get_packet_script();
	// Error _put_packet_script(const PackedByteArray &p_buffer);
	int32_t _get_packet_channel() const override;
	MultiplayerPeer::TransferMode _get_packet_mode() const override;
	void _set_transfer_channel(int32_t p_channel) override;
	int32_t _get_transfer_channel() const override;
	void _set_transfer_mode(MultiplayerPeer::TransferMode p_mode) override;
	MultiplayerPeer::TransferMode _get_transfer_mode() const override;
	void _set_target_peer(int32_t p_peer) override;
	int32_t _get_packet_peer() const override;
	bool _is_server() const override;
	void _poll() override;
	void _close() override;
	void _disconnect_peer(int32_t p_peer, bool p_force) override;
	int32_t _get_unique_id() const override;
	// void _set_refuse_new_connections(bool p_enable) override;
	// bool _is_refusing_new_connections() const override;
	bool _is_server_relay_supported() const override;
	MultiplayerPeer::ConnectionStatus _get_connection_status() const override;
	friend class MultiplexNetwork;
};
#endif
