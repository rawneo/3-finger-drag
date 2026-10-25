/* 3fgdrag.so - enable libinput native 3-finger drag under GNOME/mutter.
 *
 * mutter never calls libinput_device_config_3fg_drag_set_enabled(), so
 * libinput's default (DISABLED) stands even though driver + hardware support
 * it. We hook libinput_device_config_tap_set_enabled() (which mutter DOES
 * call for every touchpad) to switch 3-finger drag on as well, and use
 * libinput_dispatch() as a heartbeat to re-assert it if anything resets it.
 * Both hooks forward to the real functions via dlsym(RTLD_NEXT).
 */
#define _GNU_SOURCE
#include <libinput.h>
#include <dlfcn.h>
#include <stdio.h>

#define DRAG_3FG LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG

typedef enum libinput_config_status (*tap_set_fn)(struct libinput_device *,
                                                  enum libinput_config_tap_state);
typedef int (*dispatch_fn)(struct libinput *);

#define MAXDEV 4
static struct libinput_device *devs[MAXDEV];
static int ndev = 0;

static void enable_and_track(struct libinput_device *d)
{
    if (!d) return;
    if (libinput_device_config_3fg_drag_get_finger_count(d) <= 0) return;

    if (libinput_device_config_3fg_drag_get_enabled(d) != DRAG_3FG) {
        enum libinput_config_status rc =
            libinput_device_config_3fg_drag_set_enabled(d, DRAG_3FG);
        fprintf(stderr, "[3fgdrag] set 3fg=1 rc=%d now=%d on %s\n", rc,
                libinput_device_config_3fg_drag_get_enabled(d),
                libinput_device_get_name(d));
    }
    for (int i = 0; i < ndev; i++) if (devs[i] == d) return;
    if (ndev < MAXDEV) { libinput_device_ref(d); devs[ndev++] = d; }
}

enum libinput_config_status
libinput_device_config_tap_set_enabled(struct libinput_device *device,
                                       enum libinput_config_tap_state enable)
{
    static tap_set_fn real = NULL;
    if (!real) real = (tap_set_fn)dlsym(RTLD_NEXT, "libinput_device_config_tap_set_enabled");
    enum libinput_config_status st =
        real ? real(device, enable) : LIBINPUT_CONFIG_STATUS_UNSUPPORTED;
    enable_and_track(device);
    return st;
}

/* heartbeat: re-assert if anything resets the flag */
int libinput_dispatch(struct libinput *li)
{
    static dispatch_fn real = NULL;
    static unsigned long n = 0;
    if (!real) real = (dispatch_fn)dlsym(RTLD_NEXT, "libinput_dispatch");
    if ((n++ % 500) == 0) {
        for (int i = 0; i < ndev; i++) {
            if (libinput_device_config_3fg_drag_get_enabled(devs[i]) != DRAG_3FG) {
                fprintf(stderr, "[3fgdrag] WARN flag was reset -> re-enabling\n");
                libinput_device_config_3fg_drag_set_enabled(devs[i], DRAG_3FG);
            }
        }
    }
    return real ? real(li) : -1;
}
