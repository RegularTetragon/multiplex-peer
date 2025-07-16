#include "multiplex_network.h"
#include "godot_cpp/classes/cone_twist_joint3d.hpp"
#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/multiplayer_peer.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/callable.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "multiplex_packet.h"
#include "multiplex_peer.h"
#include <cstdio>

using namespace godot;

Error MultiplexNetwork::set_host_peer(Ref<MultiplayerPeer> host_peer) {
	this->host_peer = host_peer;
	internal_peers = HashMap<int32_t, Ref<MultiplexPeer>>();
	external_peers = HashMap<int32_t, int32_t>();
  host_peer->connect("peer_connected", Callable(this, "_callback_host_peer_connected"));
  host_peer->connect("peer_disconnected", Callable(this, "_callback_host_peer_disconnected"));
  return OK;
}

void MultiplexNetwork::_callback_host_peer_connected(int to_host_peer_pid) {
  printf("MUXNET - host_peer %d connected to host_peer %d\n", host_peer->get_unique_id(), to_host_peer_pid);
  if (to_host_peer_pid == 1) {
    this->external_peers.insert(1, 1);
    for (auto e = this->internal_peers.begin(); e != this->internal_peers.end(); ++e) {
      send_command(MUX_CMD_ADD_PEER, e->value->get_unique_id(), 1);
    }
  }
  else {
    // Client must now initiate some requests regarding adding subpeers.
  }
}

void MultiplexNetwork::_callback_host_peer_disconnected(int to_host_peer_pid) {
  printf("MUXNET - host_peer %d disconnected from host_peer %d\n", to_host_peer_pid, host_peer->get_unique_id());
  if (to_host_peer_pid == 1) {
    for (auto e = this->internal_peers.begin(); e != this->internal_peers.end(); ++e) {
      e->value->close();
    }
    this->internal_peers.clear();
    this->external_peers.clear();
  }
  else {
    List<int> to_delete;
    for (auto e = this->external_peers.begin(); e != this->external_peers.end(); ++e) {
      if (e->value == to_host_peer_pid) {
        to_delete.push_back(e->key);
      }
    }
    for (auto e = to_delete.begin(); e != to_delete.end(); ++e) {
      if (this->internal_peers.has(1)) {
        this->internal_peers.get(1)->emit_signal("peer_disconnected", *e);
      }
      this->external_peers.erase(*e);
    }
  }
}

MultiplexNetwork::~MultiplexNetwork() {
	this->host_peer->close();
	for (HashMap<int32_t, Ref<MultiplexPeer>>::Iterator E = this->internal_peers.begin(); E; ++E) {
		E->value->_close();
	}
	this->external_peers.clear();
	this->internal_peers.clear();
}

Error MultiplexNetwork::send(
		Ref<MultiplexPacket> packet,
		int32_t peer_id,
		int32_t channel,
		MultiplayerPeer::TransferMode transfer_mode) {
	if (this->internal_peers.has(peer_id)) {
		Ref<MultiplexPeer> peer;
		peer = this->internal_peers.get(peer_id);
		peer->_put_multiplex_packet_direct(packet);
		return OK;
	} else if (this->external_peers.has(peer_id)) {
		this->host_peer->set_transfer_mode(transfer_mode);
		this->host_peer->set_target_peer(this->external_peers.get(peer_id));
		this->host_peer->set_transfer_channel(channel);
		return this->host_peer->put_packet(packet->serialize());
	} else {
		ERR_FAIL_V_MSG(godot::ERR_CANT_CONNECT, "No known peer for peer_id");
	}
}

bool MultiplexNetwork::is_peer_connected(int32_t mux_peer_id) {
	return this->internal_peers.has(mux_peer_id) || this->external_peers.has(mux_peer_id);
}

Error MultiplexNetwork::disconnect_peer(int32_t mux_peer_id, bool force) {
	ERR_FAIL_COND_V_MSG(!this->internal_peers.has(mux_peer_id), ERR_CANT_CONNECT, "Cannot close peer that is not connected locally");

	Ref<MultiplexPeer> peer = this->internal_peers.get(mux_peer_id);
	peer->close();
	return OK;
}

void MultiplexNetwork::poll() {
  Error error = godot::OK;
	this->host_peer->poll();
  if (this->host_peer->get_available_packet_count() == 0) {
    return;
  }
	while (this->host_peer->get_available_packet_count()) {
    // printf("MUXNET - Num packets: %d\n", this->host_peer->get_available_packet_count());
		int32_t sender_host_peer_pid = this->host_peer->get_packet_peer();
		PackedByteArray packet = this->host_peer->get_packet();
    error = this->host_peer->get_packet_error();
    ERR_CONTINUE_MSG(error != OK, "Error when getting packet.");
		Ref<MultiplexPacket> multiplex_packet = Ref<MultiplexPacket>(memnew(MultiplexPacket));
		error = multiplex_packet->deserialize(packet);
    // printf("MUXNET - packet retrieved\n");
		ERR_CONTINUE_MSG(
				error != OK, "Deserializing packet failed.");
		// These packets are received from a remote network
		// They inform this network that a change has occurred
		// Control packets are handled at the MultiplexNetwork level
		// Data packets are put into their corresponding network packet
		switch (multiplex_packet->subtype) {
			case MUX_CMD:
				if (host_peer->get_unique_id() == 1) {
					handle_command_dom(sender_host_peer_pid, multiplex_packet);
				} else {
					handle_command_sub(sender_host_peer_pid, multiplex_packet);
				}
				break;
			case MUX_DATA:
				ERR_CONTINUE_MSG(
						multiplex_packet->contents.data.mux_peer_dest != 0 && !internal_peers.has(multiplex_packet->contents.data.mux_peer_dest),
						"Multiplex destination peer id is not available locally");
				ERR_CONTINUE_MSG(
						!external_peers.has(multiplex_packet->contents.data.mux_peer_source),
						"Multiplex source peer id is not associated with any remote host_peer");
				ERR_CONTINUE_MSG(
						external_peers.get(multiplex_packet->contents.data.mux_peer_source) != sender_host_peer_pid,
						"Multiplex source peer id is not associated with the provided host_peer peer id. Possible attempt at cheating.");
				internal_peers.get(multiplex_packet->contents.data.mux_peer_dest)->_put_multiplex_packet_direct(multiplex_packet);
				break;
		}
	}
  // printf("MUXNET - All packets processed.");
}

Error MultiplexNetwork::handle_command_dom(int32_t sender_pid, Ref<MultiplexPacket> multiplex_packet) {
	printf("MUXNET - DOM - Received command %d from host_peer %d about peer %d.\n", multiplex_packet->contents.command.subtype, sender_pid, multiplex_packet->contents.command.subject_multiplex_peer);
  switch (multiplex_packet->contents.command.subtype) {
		case MUX_CMD_ADD_PEER: {
      printf("MUXNET - DOM - Received command MUX_CMD_ADD_PEER\n");
			// register peer and ack
			// make sure no existing peer has the requested unique_id
			if (internal_peers.has(multiplex_packet->contents.command.subject_multiplex_peer) || external_peers.has(multiplex_packet->contents.command.subject_multiplex_peer)) {
				send_command(
            MUX_CMD_ERR_SUBPEER_ID_EXISTS, 
            multiplex_packet->contents.command.subject_multiplex_peer, 
            sender_pid);
				ERR_FAIL_V_MSG(godot::ERR_ALREADY_EXISTS, "Server rejected add peer request: peer with requested id already exists");
			}
			// make sure the number of existing peers containing the sender's base pid is less than the max
			uint32_t count = 0;
			if (max_subpeers != 0) {
				for (HashMap<int32_t, int32_t>::Iterator E = external_peers.begin(); E; ++E) {
					if (E->value == sender_pid) {
						count++;
						if (count >= max_subpeers) {
              send_command(
                  MUX_CMD_ERR_SUBPEERS_EXCEEDED, 
                  multiplex_packet->contents.command.subject_multiplex_peer, 
                  sender_pid);
				      ERR_FAIL_V_MSG(godot::ERR_CANT_CREATE, "Server rejected add peer request: peer with requested id already exists");
							break;
						}
					}
				}
			}
      printf("MUXNET - DOM - Inserting peer into external list\n");
      external_peers.insert(
          multiplex_packet->contents.command.subject_multiplex_peer,
          sender_pid);
      printf("MUXNET - DOM - Telling peer 1 about the connection\n");
      internal_peers.get(1)->emit_signal("peer_connected", multiplex_packet->contents.command.subject_multiplex_peer);
      printf("MUXNET - DOM - Sending ACK\n");
      return send_command(MUX_CMD_ADD_PEER_ACK, multiplex_packet->contents.command.subject_multiplex_peer, sender_pid);
		}
    case MUX_CMD_REMOVE_PEER: {
      int subject = multiplex_packet->contents.command.subject_multiplex_peer;
      printf("MUXNET - DOM - Request to remove subpeer %d\n", subject);
      if (sender_pid != host_peer->get_unique_id()) {
        ERR_FAIL_COND_V_MSG(!external_peers.has(subject), godot::ERR_DOES_NOT_EXIST, "MUXNET - DOM - Requested peer to remove does not exist.");
        ERR_FAIL_COND_V_MSG(external_peers.get(subject) != sender_pid, godot::ERR_UNAUTHORIZED, "MUXNET - Peer removal command was not sent by owner of peer.");
        internal_peers.get(1)->emit_signal("peer_disconnected", subject);
        external_peers.erase(subject);
      }
      else {
        ERR_FAIL_COND_V_MSG(!internal_peers.has(subject), godot::ERR_DOES_NOT_EXIST, "MUXNET - DOM - Requested peer to remove does not exist.");
        internal_peers.get(1)->emit_signal("peer_disconnected", subject);
        internal_peers.erase(subject);
      }
			break;
    }
		default:
			ERR_FAIL_V_MSG(godot::ERR_INVALID_PARAMETER, "MUXNET - DOM - Does not respond to provided command");
	}
	ERR_FAIL_V_MSG(godot::ERR_BUG, "Server handle command reached unreachable line. What!?");
}

Error MultiplexNetwork::handle_command_sub(int32_t sender_pid, Ref<MultiplexPacket> multiplex_packet) {
	printf("MUXNET - SUB - Received command %d from host_peer %d about peer %d\n", multiplex_packet->contents.command.subtype, sender_pid, multiplex_packet->contents.command.subject_multiplex_peer);
  switch (multiplex_packet->contents.command.subtype) {
		case MUX_CMD_ADD_PEER:
			external_peers.insert(
					multiplex_packet->contents.command.subject_multiplex_peer,
					sender_pid);
      return godot::OK;
		case MUX_CMD_ADD_PEER_ACK:
			// Always client receiving from server
			ERR_FAIL_COND_V_MSG(
					!internal_peers.has(
							multiplex_packet->contents.command.subject_multiplex_peer),
					godot::ERR_DOES_NOT_EXIST,
					"Subject peer of ACK is not local.");
			internal_peers.get(multiplex_packet->contents.command.subject_multiplex_peer)->complete_connection();
      return godot::OK;
    case MUX_CMD_REMOVE_PEER: {
      // Server is telling us they have removed this peer
      int subject = multiplex_packet->contents.command.subject_multiplex_peer;
      printf("MUXNET - SUB - Server forced removed peer %d", subject);
      ERR_FAIL_COND_V_MSG(!internal_peers.has(subject), ERR_DOES_NOT_EXIST, "MUXNET - SUB - Peer to remove not present");
      internal_peers.get(subject)->emit_signal("peer_disconnected", 1);
      internal_peers.get(subject)->active_mode = MultiplexPeer::MODE_NONE;
      internal_peers.get(subject)->incoming_packets.clear();
      internal_peers.erase(subject);
    }
		case MUX_CMD_ERR_SUBPEERS_EXCEEDED:
			internal_peers.get(multiplex_packet->contents.command.subject_multiplex_peer)->close();
			ERR_FAIL_V_MSG(godot::ERR_CANT_CONNECT, "MUXNET - SUB - received add peer error: maximum subpeers exceeded.");
      return godot::OK;
		case MUX_CMD_ERR_SUBPEER_ID_EXISTS:
			internal_peers.get(multiplex_packet->contents.command.subject_multiplex_peer)->close();
			ERR_FAIL_V_MSG(godot::ERR_CANT_CONNECT, "MUXNET - SUB - Client received add peer error: subpeer ID collision.");
		default:
			ERR_FAIL_V_MSG(godot::ERR_INVALID_PARAMETER, "MUXNET - SUB - Client mode MultiplexNetworks don't respond to provided command");
	}
  ERR_FAIL_V_MSG(godot::ERR_BUG, "MUXNET - SUB - Client handle command reached unreachable line. What!?");
}

Error MultiplexNetwork::_register_mux_peer(MultiplexPeer *peer) {
  ERR_FAIL_COND_V_MSG(internal_peers.has(peer->get_unique_id()),ERR_ALREADY_EXISTS,"Local peer with pid already exists");
  ERR_FAIL_COND_V_EDMSG(host_peer.is_null(), godot::ERR_DOES_NOT_EXIST, "host_peer not set");
  ERR_FAIL_COND_V_EDMSG(!host_peer.is_valid(), godot::ERR_UNCONFIGURED, "host_peer registered but not valid");
  printf("MUXNET - registering internal mux peer %d\n", peer->get_unique_id());
  this->internal_peers.insert(peer->get_unique_id(), Ref<MultiplexPeer>(peer));
  if (host_peer->get_unique_id() != 1 && host_peer->get_connection_status() == godot::MultiplayerPeer::CONNECTION_CONNECTED) {
    printf("MUXNET - requesting add of late subpeer");
    send_command(MUX_CMD_ADD_PEER, peer->get_unique_id(), 1);
  }
  return OK;
}

void MultiplexNetwork::_remove_mux_peer(MultiplexPeer *peer) {
  internal_peers.erase(peer->get_unique_id());
}

Error MultiplexNetwork::send_command(MultiplexPacketCommandSubtype subtype, int32_t subject_multiplex_peer, int32_t to_host_peer_pid) {
  printf("MUXNET - sending command %d about %d to %d\n", subtype, subject_multiplex_peer, to_host_peer_pid);
  Ref<MultiplexPacket> packet = Ref<MultiplexPacket>(memnew(MultiplexPacket));
  packet->subtype = MUX_CMD;
  packet->transfer_mode = godot::MultiplayerPeer::TRANSFER_MODE_RELIABLE;
  packet->contents.command.subtype = subtype;
  packet->contents.command.subject_multiplex_peer = subject_multiplex_peer;
  if (to_host_peer_pid == host_peer->get_unique_id()) {
    // loopback;
    if (this->host_peer->get_unique_id() == 1) {
      return this->handle_command_dom(this->host_peer->get_unique_id(), packet);
    }
    else {
      return this->handle_command_sub(this->host_peer->get_unique_id(), packet);
    }
  }
  else {
    host_peer->set_target_peer(to_host_peer_pid);
    host_peer->set_transfer_channel(1);
    host_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
    return host_peer->put_packet(packet->serialize());
  }
}

Ref<MultiplayerPeer> MultiplexNetwork::_get_host_peer() {
  return host_peer;
}

int MultiplexNetwork::get_host_peer_id_from_subpeer_id(int subpeer_id) {
  if (internal_peers.has(subpeer_id)) {
    return host_peer->get_unique_id();
  }
  else if (external_peers.has(subpeer_id)) {
    return external_peers.get(subpeer_id);
  }
  return -1;
}

void MultiplexNetwork::_bind_methods() {
  ClassDB::bind_method(D_METHOD("set_host_peer", "host_peer"), &MultiplexNetwork::set_host_peer);
  ClassDB::bind_method(D_METHOD("_callback_host_peer_connected", "to_host_peer_pid"), &MultiplexNetwork::_callback_host_peer_connected);
  ClassDB::bind_method(D_METHOD("_callback_host_peer_disconnected", "to_host_peer_pid"), &MultiplexNetwork::_callback_host_peer_disconnected);
  ClassDB::bind_method(D_METHOD("get_host_peer_id_from_subpeer_id", "subpeer_id"), &MultiplexNetwork::get_host_peer_id_from_subpeer_id);
}

