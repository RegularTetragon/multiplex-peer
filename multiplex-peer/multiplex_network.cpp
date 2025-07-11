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

Error MultiplexNetwork::set_interface(Ref<MultiplayerPeer> interface) {
	this->interface = interface;
	internal_peers = HashMap<int32_t, Ref<MultiplexPeer>>();
	external_peers = HashMap<int32_t, int32_t>();
  interface->connect("peer_connected", Callable(this, "_callback_interface_connected"));
  return OK;
}

void MultiplexNetwork::_callback_interface_connected(int to_interface_pid) {
  printf("MUXNET - interface %d connected to interface %d\n", interface->get_unique_id(), to_interface_pid);
  if (to_interface_pid == 1) {
    this->external_peers.insert(1, 1);
    for (auto e = this->internal_peers.begin(); e != this->internal_peers.end(); ++e) {
      send_command(MUX_CMD_ADD_PEER, e->value->get_unique_id(), 1);
    }
  }
  else {
    // Client must now initiate some requests regarding adding subpeers.
  }
}

MultiplexNetwork::~MultiplexNetwork() {
	this->interface->close();
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
		this->interface->set_transfer_mode(transfer_mode);
		this->interface->set_target_peer(this->external_peers.get(peer_id));
		this->interface->set_transfer_channel(channel);
		return this->interface->put_packet(packet->serialize());
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
	this->interface->poll();
  if (this->interface->get_available_packet_count() == 0) {
    return;
  }
	while (this->interface->get_available_packet_count()) {
    printf("MUXNET - Num packets: %d\n", this->interface->get_available_packet_count());
		int32_t sender_interface_pid = this->interface->get_packet_peer();
		PackedByteArray packet = this->interface->get_packet();
    error = this->interface->get_packet_error();
    ERR_CONTINUE_MSG(error != OK, "Error when getting packet.");
		Ref<MultiplexPacket> multiplex_packet = Ref<MultiplexPacket>(memnew(MultiplexPacket));
		error = multiplex_packet->deserialize(packet);
    printf("MUXNET - packet retrieved\n");
		ERR_CONTINUE_MSG(
				error != OK, "Deserializing packet failed.");
		// These packets are received from a remote network
		// They inform this network that a change has occurred
		// Control packets are handled at the MultiplexNetwork level
		// Data packets are put into their corresponding network packet
		switch (multiplex_packet->subtype) {
			case MUX_CMD:
				if (interface->get_unique_id() == 1) {
					handle_command_dom(sender_interface_pid, multiplex_packet);
				} else {
					handle_command_sub(sender_interface_pid, multiplex_packet);
				}
				break;
			case MUX_DATA:
				ERR_CONTINUE_MSG(
						multiplex_packet->contents.data.mux_peer_dest != 0 && !internal_peers.has(multiplex_packet->contents.data.mux_peer_dest),
						"Multiplex destination peer id is not available locally");
				ERR_CONTINUE_MSG(
						!external_peers.has(multiplex_packet->contents.data.mux_peer_source),
						"Multiplex source peer id is not associated with any remote interface");
				ERR_CONTINUE_MSG(
						external_peers.get(multiplex_packet->contents.data.mux_peer_source) != sender_interface_pid,
						"Multiplex source peer id is not associated with the provided interface peer id. Possible attempt at cheating.");
				internal_peers.get(multiplex_packet->contents.data.mux_peer_dest)->_put_multiplex_packet_direct(multiplex_packet);
				break;
		}
	}
  printf("MUXNET - All packets processed.");
}

Error MultiplexNetwork::handle_command_dom(int32_t sender_pid, Ref<MultiplexPacket> multiplex_packet) {
	printf("MUXNET - DOM - Received command %d from interface %d about peer %d.\n", multiplex_packet->contents.command.subtype, sender_pid, multiplex_packet->contents.command.subject_multiplex_peer);
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
		case MUX_CMD_REMOVE_PEER:
			ERR_FAIL_V_MSG(godot::ERR_UNAVAILABLE, "peer removing not implemented yet");
			break;
		default:
			ERR_FAIL_V_MSG(godot::ERR_INVALID_PARAMETER, "Server does not respond to provided command");
	}
	ERR_FAIL_V_MSG(godot::ERR_BUG, "Server handle command reached unreachable line. What!?");
}

Error MultiplexNetwork::handle_command_sub(int32_t sender_pid, Ref<MultiplexPacket> multiplex_packet) {
	printf("MUXNET - SUB - Received command %d from interface %d about peer %d\n", multiplex_packet->contents.command.subtype, sender_pid, multiplex_packet->contents.command.subject_multiplex_peer);
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
		case MUX_CMD_ERR_SUBPEERS_EXCEEDED:
			internal_peers.get(multiplex_packet->contents.command.subject_multiplex_peer)->close();
			ERR_FAIL_V_MSG(godot::ERR_CANT_CONNECT, "Client received add peer error: maximum subpeers exceeded.");
      return godot::OK;
		case MUX_CMD_ERR_SUBPEER_ID_EXISTS:
			internal_peers.get(multiplex_packet->contents.command.subject_multiplex_peer)->close();
			ERR_FAIL_V_MSG(godot::ERR_CANT_CONNECT, "Client received add peer error: subpeer ID collision.");
		default:
			ERR_FAIL_V_MSG(godot::ERR_INVALID_PARAMETER, "Client mode MultiplexNetworks don't respond to provided command");
	}
  ERR_FAIL_V_MSG(godot::ERR_BUG, "Client handle command reached unreachable line. What!?");
}

Error MultiplexNetwork::_register_mux_peer(MultiplexPeer *peer) {
  ERR_FAIL_COND_V_MSG(internal_peers.has(peer->get_unique_id()),ERR_ALREADY_EXISTS,"Local peer with pid already exists");
  ERR_FAIL_COND_V_EDMSG(interface.is_null(), godot::ERR_DOES_NOT_EXIST, "Interface not set");
  ERR_FAIL_COND_V_EDMSG(!interface.is_valid(), godot::ERR_UNCONFIGURED, "Interface registered but not valid");
  printf("MUXNET - registering internal mux peer %d\n", peer->get_unique_id());
  this->internal_peers.insert(peer->get_unique_id(), Ref<MultiplexPeer>(peer));
  if (interface->get_unique_id() != 1 && interface->get_connection_status() == godot::MultiplayerPeer::CONNECTION_CONNECTED) {
    printf("MUXNET - requesting add of late subpeer");
    send_command(MUX_CMD_ADD_PEER, peer->get_unique_id(), 1);
  }
  return OK;
}

void MultiplexNetwork::_remove_mux_peer(MultiplexPeer *peer) {
  internal_peers.erase(peer->get_unique_id());
}

Error MultiplexNetwork::send_command(MultiplexPacketCommandSubtype subtype, int32_t subject_multiplex_peer, int32_t to_interface_pid) {
  printf("MUXNET - sending command %d about %d to %d\n", subtype, subject_multiplex_peer, to_interface_pid);
  Ref<MultiplexPacket> packet = Ref<MultiplexPacket>(memnew(MultiplexPacket));
  packet->subtype = MUX_CMD;
  packet->transfer_mode = godot::MultiplayerPeer::TRANSFER_MODE_RELIABLE;
  packet->contents.command.subtype = subtype;
  packet->contents.command.subject_multiplex_peer = subject_multiplex_peer;
  
  interface->set_target_peer(to_interface_pid);
  interface->set_transfer_channel(1);
  interface->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
  return interface->put_packet(packet->serialize());
}

Ref<MultiplayerPeer> MultiplexNetwork::_get_interface() {
  return interface;
}

void MultiplexNetwork::_bind_methods() {
  ClassDB::bind_method(D_METHOD("set_interface", "interface"), &MultiplexNetwork::set_interface);
  ClassDB::bind_method(D_METHOD("_callback_interface_connected", "to_interface_pid"), &MultiplexNetwork::_callback_interface_connected);
}
