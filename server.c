#include <wayland-server-protocol.h>
#include <wayland-server-core.h>

#include <stdio.h>
#include <stdint.h>

#include <unistd.h>

struct wl_server {
	struct wl_display *display;
	struct wl_event_loop *loop;
	struct wl_event_source *key_source;
	const char *socket;
};

int wl_event_key(int fd, uint32_t mask, void *data) {
	char byte;
	struct wl_server *server = data;

	read(fd, &byte, 1);

	if(byte == 'q') {
		wl_display_terminate(server->display);
	}

	return 0;
}

void wl_compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	printf("Client Asked to make a region\n");
}


void wl_compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	printf("Client Asked to make a surface\n");
}

static void wl_compositor_handle_resource_destroy(struct wl_resource *resource) {

}

struct wl_compositor_interface compositor_impl = {
	.create_region = wl_compositor_create_region,
	.create_surface = wl_compositor_create_surface,
};
static void wl_compositor_handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_server *server = data;
	struct wl_resource *resource;

	printf("wl_client has tried to bind the compositor\n");

	resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	
	wl_resource_set_implementation(resource, &compositor_impl, NULL, wl_compositor_handle_resource_destroy);

}

int main() {
	struct wl_server state;

	state.display = wl_display_create();
	if(!state.display) {
		fprintf(stderr, "Failed to create wl display\n");
		return 1;
	}

	state.socket = wl_display_add_socket_auto(state.display);
	if(!state.socket) {
		fprintf(stderr, "Failed to get WL socket\n");
		return 1;
	}

	printf("Wayland display running on: %s\n", state.socket);

	state.loop = wl_display_get_event_loop(state.display);

	state.key_source = wl_event_loop_add_fd(state.loop, STDIN_FILENO, 
			WL_EVENT_READABLE, wl_event_key, &state);

	wl_global_create(state.display, &wl_compositor_interface, 6, &state, wl_compositor_handle_bind);

	wl_display_init_shm(state.display);


	wl_display_run(state.display);


	return 0;
}
