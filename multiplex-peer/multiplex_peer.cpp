#include "multiplex_peer.h"
#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/multiplayer_peer.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "multiplex_network.h"
#include "multiplex_packet.h"
#include <godot_cpp/variant/utility_functions.hpp>

MultiplexPeer::MultiplexPeer() {
	this->active_mode = MODE_NONE;
  this->connection_status = CONNECTION_DISCONNECTED;
	this->target_peer = 0;
  this->unique_id = 0;
}
MultiplexPeer::~MultiplexPeer() {
	if (active_mode != MODE_NONE) {
		this->close();
	}
}
Error MultiplexPeer::create_server(Ref<MultiplexNetwork> network, int max_players) {
	this->active_mode = MultiplexPeer::Mode::MODE_SERVER;
  this->connection_status = CONNECTION_CONNECTED;
	this->network = network;
  this->unique_id = 1;
  this->max_players = max_players;
  return network->_register_mux_peer(this);
}
Error MultiplexPeer::create_client(Ref<MultiplexNetwork> network) {
	this->active_mode = MultiplexPeer::Mode::MODE_CLIENT;
  this->connection_status = CONNECTION_CONNECTING;
	this->network = network;
  this->unique_id = generate_unique_id();
  return network->_register_mux_peer(this);
}
Error MultiplexPeer::_get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size) {
	ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, ERR_UNAVAILABLE, "No incoming packets available.");
  // printf("MUXNET - PEER - %d get packet called\n", unique_id);
	this->current_packet = incoming_packets.front()->get();
	incoming_packets.pop_front();
  

	*r_buffer = this->current_packet->contents.data.data;
	*r_buffer_size = this->current_packet->contents.data.length;

	return OK;
}
Error MultiplexPeer::_put_packet(const uint8_t *p_buffer, int32_t p_buffer_size) {
  // printf("MUXNET - PEER - %d put packet called, target is %d\n", unique_id, this->target_peer);
	ERR_FAIL_COND_V_MSG(active_mode == MODE_NONE, ERR_UNCONFIGURED, "Peer is not in a MultiplexNetwork");
	ERR_FAIL_COND_V_MSG(
			this->target_peer != 0 &&
					!this->network->is_peer_connected(this->target_peer),
			ERR_UNAVAILABLE,
			"No known route to peer");
	Ref<MultiplexPacket> packet = Ref<MultiplexPacket>(memnew(MultiplexPacket()));
	packet->subtype = MUX_DATA;
	packet->transfer_mode = current_transfer_mode;
	packet->contents.data.mux_peer_source = this->_get_unique_id();
	packet->contents.data.mux_peer_dest = this->target_peer;
	packet->contents.data.length = p_buffer_size;
  packet->contents.data.data = (uint8_t*)malloc(p_buffer_size);
	memcpy(packet->contents.data.data, p_buffer, p_buffer_size);
	return network->send(packet, target_peer, current_channel, current_transfer_mode);
}
void MultiplexPeer::_poll() {
  //printf("MUXNET - PEER - %d poll called\n", unique_id);
  if (network->host_peer->get_unique_id() == 1 && get_connection_status() == CONNECTION_CONNECTING) {
    complete_connection();
  }
	this->network->poll();
}
int32_t MultiplexPeer::_get_available_packet_count() const {
	return this->incoming_packets.size();
}

int32_t MultiplexPeer::_get_max_packet_size() const {
	return this->network.is_null()						? 0
			: this->network->_get_host_peer().is_null() ? 0
														: MAX_MULTIPLEX_PACKET_SIZE;
}

int32_t MultiplexPeer::_get_packet_channel() const {
	return this->current_channel;
}

void MultiplexPeer::_set_transfer_channel(int32_t value) {
	this->current_channel = value;
}

int32_t MultiplexPeer::_get_transfer_channel() const {
	return this->current_channel;
}

MultiplayerPeer::TransferMode MultiplexPeer::_get_transfer_mode() const {
	return this->current_transfer_mode;
}

void MultiplexPeer::_set_transfer_mode(MultiplayerPeer::TransferMode value) {
	this->current_transfer_mode = value;
}

bool MultiplexPeer::_is_server() const {
	return this->active_mode == MODE_SERVER;
}

int32_t MultiplexPeer::_get_packet_peer() const {
  ERR_FAIL_COND_V_MSG(active_mode == MODE_NONE, 1, "The multiplayer instance isn't currently active.");
	ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, 1, "No packets to receive.");
  auto peer = this->incoming_packets.front()->get()->contents.data.mux_peer_source;
  // printf("MUXNET - PEER - %d get packet peer was called. sender: %d\n", unique_id, peer);
  return peer;
}

void MultiplexPeer::_close() {
  printf("MUXNET - Closing peer %d", get_unique_id());
	if (this->active_mode == MODE_NONE) {
		return;
	}
	incoming_packets.clear();
	this->active_mode = MODE_NONE;
  
  if (unique_id == 1) {
    for (auto e = network->internal_peers.begin(); e != network->internal_peers.end(); ++e) {
      emit_signal("peer_disconnected", e->key);
    }
    for (auto e = network->external_peers.begin(); e != network->external_peers.end(); ++e) {
      emit_signal("peer_disconnected", e->key);
    }
    network->host_peer->close();
  }
  else {
	  this->network->send_command(MUX_CMD_REMOVE_PEER, this->_get_unique_id(), 1);
    emit_signal("peer_disconnected", 1);
    
  }
  this->network->internal_peers.erase(get_unique_id());
  connection_status = CONNECTION_DISCONNECTED;
}

void MultiplexPeer::_disconnect_peer(int32_t p_peer, bool p_force) {
  if (this->network->external_peers.has(p_peer)) {
    if (p_peer == 1) {
      // If the server disconnected from us then we should probably close.
      this->close();
    }
    else if (unique_id == 1) {
      // If we're the server, tell the client to remove the subpeer, and remove it from our known external peers.
      this->network->send_command(MUX_CMD_REMOVE_PEER, p_peer, this->network->external_peers.get(p_peer));
      this->network->external_peers.erase(p_peer);
    }
  }
  if (!p_force) {
    emit_signal("peer_disconnected", p_peer);
  }
}

MultiplayerPeer::ConnectionStatus MultiplexPeer::_get_connection_status() const {
	return connection_status;
}

MultiplayerPeer::TransferMode MultiplexPeer::_get_packet_mode() const {
	ERR_FAIL_COND_V_MSG(active_mode == MODE_NONE, TRANSFER_MODE_RELIABLE, "The multiplayer instance isn't currently active.");
	ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, TRANSFER_MODE_RELIABLE, "No pending packets, cannot get transfer mode.");

	return incoming_packets.front()->get()->transfer_mode;
}

bool MultiplexPeer::_is_server_relay_supported() const {
	ERR_FAIL_COND_V_MSG(this->network.is_null(), 0, "MultiplexPeer has no associated network.");
	ERR_FAIL_COND_V_MSG(this->network->_get_host_peer().is_null(), 0, "MultiplexNetwork has no host_peer");
	return this->network->_get_host_peer()->is_server_relay_supported();
}

Error MultiplexPeer::_put_multiplex_packet_direct(Ref<MultiplexPacket> packet) {
	this->incoming_packets.push_back(packet);
	return OK;
}

void MultiplexPeer::_set_target_peer(int32_t p_peer) {
	target_peer = p_peer;
}

int32_t MultiplexPeer::_get_unique_id() const {
	return unique_id;
}

void MultiplexPeer::_bind_methods() {
  ClassDB::bind_method(D_METHOD("create_server", "network", "num_players"), &MultiplexPeer::create_server);
  ClassDB::bind_method(D_METHOD("create_client", "network"), &MultiplexPeer::create_client);
  ClassDB::bind_method(D_METHOD("complete_connection"), &MultiplexPeer::complete_connection);
}

void MultiplexPeer::complete_connection() {
  connection_status = CONNECTION_CONNECTED;
  if (unique_id != 1) {
    emit_signal("peer_connected", 1);
    if (network->internal_peers.has(1)) {
      network->internal_peers.get(1)->emit_signal("peer_connected", unique_id);
    }
  }
}

