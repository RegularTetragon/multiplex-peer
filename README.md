# Godot Multiplex Peer
NOTE: This library is in very active development and is likely not production ready. You may run into some bugs if you use this. If you run into any trouble please make an issue!

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

# Adding to your project

You can git clone this repo from the master branch straight into your addons directory, add it as a submodule, or unzip this repo
into your addons folder. This is the primarily supported way of running this addon for now, however all that is really needed
for godot to work with this addon are the bin folder and multiplex-peer.gdextension.

# Compiling

## Precompiled

The bin/ directory **should** contain the most recent version of this repo.

## Docker

Ideally at some point I'll port this all to use just nix, however there are currently some outstanding bugs with
nix that make that difficult. As such, the easiest way to compile for a given platform is:

```
docker-compose up --build <linux|windows|windows-32>-<debug|release>
```
depending on what platform you are trying to build for inside the base directory. This should work with a fresh pull, and will will populate bin/linux and bin/windows
with the necessary .so and dll files. I recommend running each independently instead of `docker-compose up --build`, because
doing that is very CPU intensive and may fail with unrelated error messages (at least it does on my machine).

If you prefer you can build for a single platform at a time like this

```
docker-compose up --build linux-release
```

## Manual

If you prefer not to use docker you should be able to do everything manually with scons according to
Godot's official build instructions for your platform.
There are no other dependencies aside from the `godot-cpp` submodule. You can pull this submodule the usual way:

```
git submodule update --recursive
```

A basic build command that will generate the debug version for your platform is

```
scons dev_build=yes
```
This is the command I use while developing this repo.

On NixOS specifically you can enter a `nix develop` shell and then run the desired scons command. `nix build` may or may not work.

# Usage

Normally Godot peers communicate over some network medium, such as UDP, Bluetooth, or some sort of relay service. MultiplexPeers also need a medium to communicate
over. In this case however, this medium is an object tracked by Godot called a MultiplexNetwork. A MultiplexNetwork sorts out whether messages are being sent
to local peers or to remote peers. You construct a MultiplexNetwork like so:

```
var mux_net = MultiplexNetwork.new()
```

Now a multiplex network needs a "host peer" to wrap. To clarify, this "host peer" can either be acting like a server or a client. Currently mesh peers are not supported.
```gdscript
# create client
var interface = ENetMultiplayerPeer.new()
interface.create_client(
	address,
	port
)
var mux_net = MultiplexNetwork.new()
mux_net.set_host_peer(interface)
```

```gdscript

# create server
var interface = ENetMultiplayerPeer.new()
error = interface.create_server(port, max_players)
var mux_net = MultiplexNetwork.new()
mux_net.set_host_peer(interface)
```

Now, we can make many subpeers that communicate over this mux_net. MuxNet keeps track of which of these subpeers are local and which ones are remote.
If a subpeer is local, they communicate directly without using the interface at all. If they subpeer is remote, the outgoing packet is wrapped in 
the MultiplexPeer protocol, and sent over the host peer, to be unwrapped by the receiving MuxNet on the other side.

```gdscript
# create server
var mux_server_peer = MultiplexPeer.new()
mux_server_peer.create_server(mux_net, max_players)
var server = server_prefab.instantiate()
add_child(server)
get_tree().set_multiplayer(
    MultiplayerAPI.create_default_interface(),
    server.get_path()
)
server.multiplayer.peer = mux_server_peer

for i in range(num_local_players):
    var mux_client_peer = MultiplexPeer.new()
    mux_client_peer.create_client(mux_net)
    var client = client_prefab.instantiate()
    add_child(client)

    get_tree().set_multiplayer(
        MultiplayerAPI.create_default_interface(),
        client.get_path()
    )
    client.multiplayer.peer = mux_client_peer
```

```gdscript
# create client

for i in range(num_split_screen_players):
    var mux_client_peer = MultiplexPeer.new()
    mux_client_peer.create_client(mux_net)
    var client = client_prefab.instantiate()
    get_tree().add_child(client)

    get_tree().set_multiplayer(
        MultiplayerAPI.create_default_interface(),
        client.get_path()
    )
    client.multiplayer.peer = mux_client_peer
```

Now each MultiplexPeer can effectively share a single MultiplayerPeer. This means no special considerations need to be made for host vs server mode. Each MultiplexPeer manages being opened and closed independently,
however when then interface is closed or if subpeer 1 is closed all subpeers will be closed. Keep in mind: on the client closing all subpeers will not close the host peer, you will need to issue that command separately.


# Acknowledgements

This library takes significant inspiration (and some small pieces of code) straight out of expressobits/steam-multiplayer-peer. 
