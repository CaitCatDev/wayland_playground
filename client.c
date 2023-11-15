#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Wayland client side in theroy for play*/
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

struct wl_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	struct wl_shm *shm;
	struct wl_buffer *buffer;
};

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	struct wl_state *state = data;
	printf("%d %s version: %d\n", name, interface, version);

	if(strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
	}
}

void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	printf("Global Removed: %d\n", name);
}

struct wl_registry_listener wl_registry_listener = {
	.global = wl_registry_global,
	.global_remove = wl_registry_global_remove,
};

int main(int argc, char *argv[])
{
	struct wl_state state;

	state.display = wl_display_connect(NULL);
	if(!state.display) {
		printf("wl_display_connect: %m\n");
		exit(1);
	}

	state.registry = wl_display_get_registry(state.display);
	if(!state.registry) {
		printf("wl_display_get_registry: %m\n");
		exit(1);
	}

	wl_registry_add_listener(state.registry, &wl_registry_listener, &state);

	wl_display_roundtrip(state.display);

	state.surface = wl_compositor_create_surface(state.compositor);

	while(wl_display_dispatch(state.display)) {
		
	}

	printf("%p\n", state.surface);

	return EXIT_SUCCESS;
}
