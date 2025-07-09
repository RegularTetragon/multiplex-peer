# Godot Multiplex Peer
NOTE: This library is a work in progress and is not ready for production. Many things don't work in it yet.
It currently has only been testing on Linux with local networking over the ENetMultiplayerPeer.
I finally got my game working with it in an incredibly specific context, which is why it has been released in this state.

Multiplex Peer is an implementation of a Godot MultiplayerPeerExtension that allows you to split an existing MultiplayerPeer into many multiplayer peers.
This allows for easy local splitscreen multiplayer that utilizes the same netcode as the rest of your game, without special logic for hosts.


```
Server            | Client            | Client
MuxPeer (server)  | MuxPeer (client)  | MuxPeer(client)
      \                      |                  /
                           MuxNet
                SteamMultiplayerPeer (server)
                     Steam Relay Service
                SteamMultiplayerPeer (client)
                           MuxNet
      /                      |                  \
MuxPeer (client)  | MuxPeer (client)  | MuxPeer(client)
Client            | Client            | Client
```


# Motivation
This implementation exists because I architected a game to do split screen multiplayer using multiple ENetMultiplayerPeers. Even hosting in this game is done
by creating a server and a client ENetMultiplayerPeer, then connecting those two over the local network. This works fine because ENetMultiplayerPeer is
designed in a way that you may have many of them on a scene tree or many of them on a system. Not all MultiplayerPeers are created like this though.

As I was porting a game of mine to use Steam Networking, I ran into a significant issue with a design that worked fine through ENetMultiplayerPeer.
Unfortunately, neither of the current publicly available SteamMultiplayerPeer implementations allowed you to have many SteamMultiplayerPeers on the same system.
This is because both implemenations assume that a SteamMultiplayerPeer and a connection through the Steam Relay are 1:1, and you can only have one open
connection to the steam relay per steam account, and you can only have one open steam client per OS user, and you can only have one active steam user per client.

I don't think there's anything wrong with this assumption, necessarily. However, this posed significant issues to the current design of my game.
Instead of reworking the SteamMultiplayerPeers, I decided to solve this issue more generally, because if I was porting a game to a future platform
that had a similar assumption I'd prefer to not have to patch that platform's networking peer in the same manner. As such I created this MultiplexPeer
which is capable of wrapping another peer to act as its interface to the outside world. Each side of the server/client boundry is then able to create
multiple MultiplexPeers which route packets internally through a Multiplex Network.
# Usage

My game starts up servers and clients like so:

```gdscript
func start_client_mux_enet(address: String, port: int) -> Client:
	print("GAME  - Starting client over mux'd enet ", address, " and port ", port)
	var interface = ENetMultiplayerPeer.new()
	interface.create_client(
		address, port
	)
	var mux_net = MultiplexNetwork.new()
	mux_net.set_interface(interface)

	var client_mux_peer = MultiplexPeer.new()
	client_mux_peer.create_client(mux_net)

	return start_client(client_mux_peer)


func start_server_mux_enet(
	port: int,
	max_players: int,
	local_multiplayer: int, 
	gamemode: GamemodeResource,
	level: LevelResource
):
	var interface = ENetMultiplayerPeer.new()
	var error
	error = interface.create_server(port, max_players)
	if error != OK:
		print("GAME   - Error starting server")
		main_menu.show_error(error_string(error))
		return
	

	var mux_net = MultiplexNetwork.new()
	# set interface checks whether the interface is a client or server.
	# if it is a server, it begins listening for mux_net packets and
	# forwards them to registered peers.

	# if the interface is a client, it begins a handshake with the mux on the other side
	# using the interface as an intermediate
	error = mux_net.set_interface(interface)
	if error != OK:
		print("GAME   - Error starting Mux")
		main_menu.show_error(error_string(error))
		return
	
	var mux_server_peer = MultiplexPeer.new()
	mux_server_peer.create_server(mux_net, max_players)
	
	start_server(mux_server_peer, gamemode, level)

	mux_server_peer.complete_connection()
	$GridContainer.columns = max(1, ceil(sqrt(float(local_multiplayer-1))))
	for i in range(0, local_multiplayer):
		var mux_client_peer = MultiplexPeer.new()
		error = mux_client_peer.create_client(mux_net)
		if error != OK:
			print("GAME  - Error starting client")
			main_menu.show_error(error_string(error))
			return
		var client = start_client(mux_client_peer)
		mux_client_peer.complete_connection()
		client.input_device_id = i
		if i == 0:
			client.closed.connect(
				func(_msg): 
					mux_server_peer.quit()
			)
```

This library is still in pretty deep development, so proper documentation is still pending.

# Acknowledgements

This library takes significant inspiration (and some small pieces of code) straight out of expressobits/steam-multiplayer-peer. 
