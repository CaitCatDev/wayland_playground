#include <stdint.h>
#include <wayland-util.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <wayland-server-protocol.h>
#include <wayland-server-core.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xdg-shell.h"

/*This program shouldn't be used as a guide or an example of how wayland backends
 * should work as we are not actually using any buffers we may attach and we 
 * are not following the normal way to "display" a surface we are simply 
 * displaying a whit square and making it longer for each surface we have.
 * But this was more a test of how creating surfaces works
 */
/*PS sorry for the cursed code*/
typedef struct x_backend {
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	xcb_window_t window;
	xcb_gcontext_t gc;
	
	struct wl_display *display;
	struct wl_event_loop *loop;

	struct xkb_context *context;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
} x_backend_t;

struct our_surface {
	struct wl_resource *surface;
	struct wl_resource *buffer;
	struct wl_resource *xdg_surface;
	struct wl_resource *xdg_toplevel;
	struct wl_resource *keyboard;
	x_backend_t *xcb;
};

static struct our_surface *active_surface;
static struct wl_resource *gkeyboard;

/*Called when the X server has an event for us*/
int wl_event_backend(int fd, uint32_t mask, void *data) {
	x_backend_t *backend = data;
	xcb_generic_event_t *event;
	

	while((event = xcb_poll_for_event(backend->connection))) {
		if(gkeyboard == NULL) return 0;
		switch(event->response_type) {
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *key = (void*)event;
				wl_keyboard_send_key(gkeyboard, 0, 0, key->detail - 8, 1);
				break;
			}
			case XCB_KEY_RELEASE: {
				xcb_key_release_event_t *key = (void *)event;
				wl_keyboard_send_key(gkeyboard, 0, 0, key->detail - 8, 0);
				break;
			}
			default:
				printf("Unknown X Event: %d\n", event->response_type);
				break;
		}
	}

	return 0;
}

x_backend_t *backend_init(struct wl_display *display) {
	xcb_screen_iterator_t iter;
	int screen_nbr;
	const xcb_setup_t *setup;
	x_backend_t *xcb = calloc(1, sizeof(*xcb));
	uint32_t mask, values[2];

	xcb->connection = xcb_connect(NULL, &screen_nbr);
	
	setup = xcb_get_setup(xcb->connection);

	iter = xcb_setup_roots_iterator(setup);
	
	for (; iter.rem; --screen_nbr, xcb_screen_next (&iter)) {
		if (screen_nbr == 0) {
			xcb->screen = iter.data;
			break;
		}
	}

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = 0x000000;
	values[1] =  XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
              XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
              XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;

	xcb->window = xcb_generate_id(xcb->connection);

	xcb_create_window(xcb->connection, xcb->screen->root_depth,
			xcb->window, xcb->screen->root, 0, 0, 600, 600, 1, 
			XCB_WINDOW_CLASS_INPUT_OUTPUT, xcb->screen->root_visual,
			mask, values);
	
	xcb->gc = xcb_generate_id(xcb->connection);

	mask = XCB_GC_FOREGROUND;
	values[0] = 0xf8f8f2;

	xcb_create_gc(xcb->connection, xcb->gc, xcb->window, mask, values);

	xcb->loop = wl_display_get_event_loop(display);
	xcb->display = display;

	int fd = xcb_get_file_descriptor(xcb->connection);

	wl_event_loop_add_fd(xcb->loop, fd, WL_EVENT_READABLE, wl_event_backend, xcb);

	xcb->context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	uint32_t device_id;
	
	xkb_x11_setup_xkb_extension(xcb->connection, 1, 0, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, 
			NULL, NULL, NULL, NULL);

	device_id = xkb_x11_get_core_keyboard_device_id(xcb->connection);

	xcb->keymap = xkb_x11_keymap_new_from_device(xcb->context, xcb->connection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);

	xcb->state = xkb_x11_state_new_from_device(xcb->keymap, xcb->connection, device_id);

	xcb_map_window(xcb->connection, xcb->window);
	xcb_flush(xcb->connection);
	return xcb;
}

void wl_surface_attach(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *buffer, int32_t x, int32_t y) {
	struct our_surface *surface = wl_resource_get_user_data(resource);
	surface->buffer = buffer;
}

void wl_surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback) {

}

void wl_surface_damage(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void wl_surface_set_opaque_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}

void wl_surface_set_input_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}

void wl_surface_commit(struct wl_client *client, struct wl_resource *resource) {
	struct our_surface *surface = wl_resource_get_user_data(resource);
	active_surface = surface;
	x_backend_t *xcb = surface->xcb;
	if(surface->buffer == NULL) {
		xdg_surface_send_configure(surface->xdg_surface, 0);
		return;
	}

	struct wl_shm_buffer *buffer = wl_shm_buffer_get(surface->buffer);
	uint32_t height = wl_shm_buffer_get_height(buffer);
	uint32_t width = wl_shm_buffer_get_width(buffer);
	void *data = wl_shm_buffer_get_data(buffer);

	xcb_image_t *image = xcb_image_create(width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, 32, 24, 32, 32, XCB_IMAGE_ORDER_LSB_FIRST, XCB_IMAGE_ORDER_LSB_FIRST, 
			data, width * 4 * height, data);

	xcb_rectangle_t rect = {
		.x = 50, 
		.y = 50,
		.width = 100,
		.height = 100,
	};


	xcb_image_put(xcb->connection, xcb->window, xcb->gc, image, 50, 50, 0);
	xcb_flush(xcb->connection);
	
	sleep(1);
	xdg_surface_send_configure(surface->xdg_surface, 0);
}

void wl_surface_set_buffer_transform(struct wl_client *client, 
		struct wl_resource *resource, int32_t transform) {

}

void wl_surface_set_buffer_scale(struct wl_client *client, 
		struct wl_resource *resource, int32_t scale) {

}

void wl_surface_damage_buffer(struct wl_client *client, 
		struct wl_resource *resource, int32_t x, int32_t y,
		int32_t width, int32_t height) {

}

void wl_surface_offset(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y) {

}

void wl_surface_destroy(struct wl_client *client, struct wl_resource *resource) {

}

struct wl_surface_interface wl_surface_implementation = {
	.attach = wl_surface_attach,
	.frame = wl_surface_frame,
	.commit = wl_surface_commit, 
	.damage = wl_surface_damage,
	.offset = wl_surface_offset,
	.destroy = wl_surface_destroy,
	.damage_buffer = wl_surface_damage_buffer,
	.set_buffer_scale = wl_surface_set_buffer_scale,
	.set_input_region = wl_surface_set_input_region,
	.set_opaque_region = wl_surface_set_opaque_region,
	.set_buffer_transform = wl_surface_set_buffer_transform,
};

void wl_compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

static void wl_surface_handle_resource_destroy(struct wl_resource *resource) {

}

void wl_compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	static int surfc;
	struct our_surface *surface;
	printf("Client has requested to make a compositor surface\n");

	surfc++;

	surface = calloc(1, sizeof(*surface));
	
	surface->surface = wl_resource_create(client, &wl_surface_interface, 6, id);
	surface->xcb = wl_resource_get_user_data(resource);

	wl_resource_set_implementation(surface->surface, &wl_surface_implementation, surface, wl_surface_handle_resource_destroy);
}

static void wl_compositor_handle_resource_destroy(struct wl_resource *resource) {

}

struct wl_compositor_interface wl_compositor_implementation = {
	.create_region = wl_compositor_create_region,
	.create_surface = wl_compositor_create_surface,
};
static void wl_compositor_handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_server *server = data;
	struct wl_resource *resource;

	printf("wl_client has tried to bind the compositor\n");

	resource = wl_resource_create(client, &wl_compositor_interface, version, id);

	wl_resource_set_implementation(resource, &wl_compositor_implementation,
			data, wl_compositor_handle_resource_destroy);

}

static void wl_seat_handle_resource_destroy(struct wl_resource *resource) {

}

void wl_keyboard_release(struct wl_client *client, struct wl_resource *resource) {

}

void wl_keyboard_destroy(struct wl_resource *resource) {

}

struct wl_keyboard_interface wl_keyboard_implementation = {
	.release = wl_keyboard_release,
};

void wl_seat_release(struct wl_client *client, struct wl_resource *resource) {

}

void wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource,
		uint32_t id) {
	x_backend_t *xcb;
	struct wl_resource *keyboard;

	xcb = wl_resource_get_user_data(resource);

	keyboard = wl_resource_create(client, &wl_keyboard_interface, 1, id);
	wl_resource_set_implementation(keyboard, &wl_keyboard_implementation, 
			xcb, wl_keyboard_destroy);
	

	/*Get and send keymap to the client*/
	char *kmap_str = xkb_keymap_get_as_string(xcb->keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	int size = strlen(kmap_str) + 1;
	int fd = open("/tmp", O_TMPFILE|O_RDWR|O_EXCL, 0600);
	ftruncate(fd, size);
	char *mapped_str = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	strcpy(mapped_str, kmap_str);
	munmap(mapped_str, size);

	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
	gkeyboard = keyboard;


}

void wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource,
		uint32_t id) {

}

void wl_seat_get_touch(struct wl_client *client, struct wl_resource *resource,
		uint32_t id) {

}

struct wl_seat_interface wl_seat_implementation = {
	.get_keyboard = wl_seat_get_keyboard,
	.release = wl_seat_release,
	.get_touch = wl_seat_get_touch,
	.get_pointer = wl_seat_get_pointer,
};
void wl_seat_bind(struct wl_client *client, void *data, 
		uint32_t version, uint32_t id) {
	struct wl_resource *resource;

	printf("Wl_client has bound wl_seat\n");

	resource = wl_resource_create(client, &wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &wl_seat_implementation, data, wl_seat_handle_resource_destroy);

	

	wl_seat_send_capabilities(resource, WL_SEAT_CAPABILITY_KEYBOARD);
}

void xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_set_maximised(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_set_minimised(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_unset_maximized(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *resource,
		const char *title) {

}

void xdg_toplevel_set_appid(struct wl_client *client, struct wl_resource *resource,
		const char *appid) {

}

void xdg_toplevel_show_window_menu(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *wl_seat, 
		uint32_t serial, int32_t x, int32_t y) {

}

void xdg_toplevel_move(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *wl_seat, 
		uint32_t serial) {

}

void xdg_toplevel_resize(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *wl_seat, 
		uint32_t serial, uint32_t edge) {
	
}

void xdg_toplevel_set_max_size(struct wl_client *client, 
		struct wl_resource *resource, int32_t width, int32_t height) {
	
}

void xdg_toplevel_set_min_size(struct wl_client *client, 
		struct wl_resource *resource, int32_t width, int32_t height) {
	
}

void xdg_toplevel_set_parent(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *parent) {
	
}

void xdg_toplevel_set_fullscreen(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *output) {
	
}

struct xdg_toplevel_interface xdg_toplevel_implementation = {
	.destroy = xdg_toplevel_destroy,
	.set_maximized = xdg_toplevel_set_maximised,
	.set_minimized = xdg_toplevel_set_minimised,
	.unset_fullscreen = xdg_toplevel_unset_fullscreen,
	.unset_maximized = xdg_toplevel_unset_maximized,
	.set_title = xdg_toplevel_set_title,
	.set_app_id = xdg_toplevel_set_appid,
	.show_window_menu = xdg_toplevel_show_window_menu,
	.move = xdg_toplevel_move,
	.resize = xdg_toplevel_resize,
	.set_max_size = xdg_toplevel_set_max_size,
	.set_min_size = xdg_toplevel_set_min_size,
	.set_parent = xdg_toplevel_set_parent,
	.set_fullscreen = xdg_toplevel_set_fullscreen,
};

void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_toplevel_resource_destroy(struct wl_resource *resource) {

}

void xdg_surface_get_toplevel(struct wl_client *client, 
		struct wl_resource *resource, uint32_t id) {
	struct wl_resource *toplevel = wl_resource_create(client, &xdg_toplevel_interface, 1, id);
	struct wl_array array;
	wl_array_init(&array);

	struct our_surface *surface = wl_resource_get_user_data(resource);

	wl_resource_set_implementation(toplevel, &xdg_toplevel_implementation, surface,
			xdg_toplevel_resource_destroy);
	xdg_toplevel_send_configure(toplevel, 600, 600, &array);

}

void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
		uint32_t id, struct wl_resource *parent, struct wl_resource *positioner) {

}

void xdg_surface_ack_configure(struct wl_client *client, 
		struct wl_resource *resource, uint32_t serial) {

}

void xdg_surface_set_window_geometry(struct wl_client *client, 
		struct wl_resource *resource, int32_t x, int32_t y, 
		int32_t width, int32_t height) {

}

void xdg_surface_resource_destroy(struct wl_resource *resource) {

}


struct xdg_surface_interface xdg_surface_implementation = {
	.destroy = xdg_surface_destroy,
	.get_toplevel = xdg_surface_get_toplevel,
	.ack_configure = xdg_surface_ack_configure,
	.get_popup = xdg_surface_get_popup,
	.set_window_geometry = xdg_surface_set_window_geometry,
};

void xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {
	
}

void xdg_wm_base_resource_destroy(struct wl_resource *resource) {

}

void xdg_wm_base_get_xdg_surface(struct wl_client*client, struct wl_resource *resource, uint32_t id, struct wl_resource *wl_surface) {
	struct wl_resource *xdg_surface = wl_resource_create(client, &xdg_surface_interface, 1, id);
	struct our_surface *surface = wl_resource_get_user_data(wl_surface);

	wl_resource_set_implementation(xdg_surface, &xdg_surface_implementation, surface, xdg_surface_resource_destroy);

	surface->xdg_surface = xdg_surface;
	
	
}

void xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {

}

void xdg_wm_base_create_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {

}

struct xdg_wm_base_interface xdg_wm_base_implementation = {
	.destroy = xdg_wm_base_destroy,
	.get_xdg_surface = xdg_wm_base_get_xdg_surface,
	.pong = xdg_wm_base_pong,
	.create_positioner = xdg_wm_base_create_positioner,
};

void xdg_wm_base_bind(struct wl_client *client, void *data, 
		uint32_t version, uint32_t id) {
	printf("Client has asked to bind xdg_wm_base\n");
	struct wl_resource *resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
	wl_resource_set_implementation(resource, &xdg_wm_base_implementation, 
			data, xdg_wm_base_resource_destroy);
}

int main() {
	
	struct wl_display *display = wl_display_create();
	x_backend_t *backend = backend_init(display);

	wl_display_add_socket_auto(display);

	wl_global_create(display, &wl_compositor_interface, 6, backend, wl_compositor_handle_bind);
	wl_global_create(display, &wl_seat_interface, 9, backend, wl_seat_bind);
	wl_global_create(display, &xdg_wm_base_interface, 6, backend, xdg_wm_base_bind);
	

	wl_display_init_shm(display);

	wl_display_run(display);

	return 0;
}
