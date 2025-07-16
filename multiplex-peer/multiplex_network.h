#ifndef MULTIPLEX_NETWORK_H
#define MULTIPLEX_NETWORK_H
#include "godot_cpp/classes/ref_counted.hpp"
#include "multiplex_packet.h"
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/templates/hash_map.hpp>

using namespace godot;

class MultiplexPeer;
class MultiplexNetwork : public RefCounted {
	GDCLASS(MultiplexNetwork, RefCounted)
private:
	HashMap<int32_t, Ref<MultiplexPeer>> internal_peers;
	HashMap<int32_t, int32_t> external_peers;
	Ref<MultiplayerPeer> host_peer;
	uint32_t max_subpeers; // 0 = inf
	Error handle_command_dom(int32_t sender_pid, Ref<MultiplexPacket> packet);
	Error handle_command_sub(int32_t sender_pid, Ref<MultiplexPacket> packet);
  Error send_command(MultiplexPacketCommandSubtype subtype, int32_t subject_multiplex_peer, int32_t to_host_peer_pid);
protected:
  static void _bind_methods();
public:
  void _callback_host_peer_connected(int to_host_peer_pid);
  void _callback_host_peer_disconnected(int host_peer_pid);
  int get_host_peer_id_from_subpeer_id(int subpeer_id);
	Error _register_mux_peer(MultiplexPeer *peer);
	void _remove_mux_peer(MultiplexPeer *peer);
  void _close();
	~MultiplexNetwork();
  Error set_host_peer(Ref<MultiplayerPeer> host_peer);
	Error send(Ref<MultiplexPacket> packet, int32_t peer_id, int32_t channel, MultiplayerPeer::TransferMode transfer_mode);
	bool is_peer_connected(int32_t mux_peer_id);
	Error disconnect_peer(int32_t mux_peer_id, bool force);
	void poll(); // responsible for taking packets off of host_peer, validating them, handling commands, and putting data packets into the correct MultiplexPeer queue, may be called multiple times in one frame
	Ref<MultiplayerPeer> _get_host_peer();
  friend class MultiplexPeer;
};
#endif
