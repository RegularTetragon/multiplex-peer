#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "multiplex_peer.h"
#include "multiplex_packet.h"

using namespace godot;

void initialize_multiplex_peer(ModuleInitializationLevel level) {
	if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
    ClassDB::register_class<MultiplexPeer>();
    ClassDB::register_class<MultiplexNetwork>();
    ClassDB::register_class<MultiplexPacket>();
	}
}

void uninitialize_multiplex_peer(ModuleInitializationLevel level) {
	if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT multiplex_peer_init(GDExtensionInterfaceGetProcAddress p_interface, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);

	init_obj.register_initializer(initialize_multiplex_peer);
	init_obj.register_terminator(uninitialize_multiplex_peer);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
