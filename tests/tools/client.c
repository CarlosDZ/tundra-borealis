#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

static const char *wanted[] = {"wl_compositor", "wl_subcompositor", "wl_shm",
                               "wl_data_device_manager"};
#define WANTED (sizeof(wanted) / sizeof(wanted[0]))

static int seen[WANTED];

static void on_global(void *data, struct wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
	(void)data;
	(void)registry;
	(void)name;
	(void)version;
	for (size_t i = 0; i < WANTED; i++)
		if (strcmp(interface, wanted[i]) == 0)
			seen[i] = 1;
}

static void on_global_remove(void *data, struct wl_registry *registry,
                             uint32_t name) {
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
    .global_remove = on_global_remove,
};

int main(void) {
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "cannot connect to the compositor\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "the compositor closed the connection\n");
		return 1;
	}
	int rc = 0;
	for (size_t i = 0; i < WANTED; i++) {
		if (!seen[i]) {
			fprintf(stderr, "missing global %s\n", wanted[i]);
			rc = 1;
		}
	}
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
	return rc;
}
