/***
  This file is part of PulseAudio.

  Copyright 2013 David Henningsson, Canonical Ltd.

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core-util.h>
#include <pulsecore/device-port.h>
#include <pulsecore/i18n.h>
#include <libudev.h>

#include "alsa-util.h"
#include "alsa-extcon.h"

/* IFDEF HAVE_UCM ? */
#include <use-case.h>
#include "alsa-ucm.h"
/* ENDIF */

/* For android */
#define EXTCON_NAME "switch"
/* For extcon */
/* #define EXTCON_NAME "extcon" */


/* TODO: Backport stuff to 4.0, remove before upstreaming */
#ifndef PA_PORT_AVAILABLE_YES
#define PA_PORT_AVAILABLE_YES PA_AVAILABLE_YES
#define PA_PORT_AVAILABLE_NO PA_AVAILABLE_NO
#define PA_PORT_AVAILABLE_UNKNOWN PA_AVAILABLE_UNKNOWN
#define pa_port_available_t pa_available_t
#endif

static pa_port_available_t hp_avail(int state)
{
    return ((state & 3) != 0) ? PA_PORT_AVAILABLE_YES : PA_PORT_AVAILABLE_NO;
}

static pa_port_available_t hsmic_avail(int state)
{
    return (state & 1) ? PA_PORT_AVAILABLE_YES : PA_PORT_AVAILABLE_NO;
}

struct android_switch {
    char *name;
    uint32_t current_value;
};

static void android_switch_free(struct android_switch *as) {
    if (!as)
        return;
    pa_xfree(as->name);
    pa_xfree(as);
}

static struct android_switch *android_switch_new(const char *name) {

    struct android_switch *as = NULL;
    char *filename = pa_sprintf_malloc("/sys/class/%s/%s/state", EXTCON_NAME, name);
    char *state = pa_read_line_from_file(filename);

    if (state == NULL) {
        pa_log_debug("Cannot open '%s'. Skipping.", filename);
        pa_xfree(filename);
        return NULL;
    }
    pa_xfree(filename);

    as = pa_xnew0(struct android_switch, 1);
    as->name = pa_xstrdup(name);

    if (pa_atou(state, &as->current_value) < 0) {
        pa_log_warn("Switch '%s' has invalid value '%s'", name, state);
        pa_xfree(state);
        android_switch_free(as);
        return NULL;
    }

    return as;
}

struct udev_data {
    struct udev *udev;
    struct udev_monitor *monitor;
    pa_io_event *event;
};

struct pa_alsa_extcon {
    pa_card *card;
    struct android_switch *h2w;
    struct udev_data udev;
};

static struct android_switch *find_matching_switch(pa_alsa_extcon *u,
                                                   const char *devpath) {

    if (pa_streq(devpath, "/devices/virtual/" EXTCON_NAME "/h2w"))
        return u->h2w;  /* To be extended if we ever support more switches */
    return NULL;
}

static void notify_ports(pa_alsa_extcon *u, struct android_switch *as) {

    pa_device_port *p;
    void *state;

    pa_assert(as == u->h2w); /* To be extended if we ever support more switches */

    pa_log_debug("Value of switch %s is now %d.", as->name, as->current_value);

    PA_HASHMAP_FOREACH(p, u->card->ports, state) {
        if (p->is_output) {
            if (!strcmp(p->name, "analog-output-headphones"))
                 pa_device_port_set_available(p, hp_avail(as->current_value));
/* IFDEF HAVE_UCM ? */
            else if (pa_alsa_ucm_port_contains(p->name, SND_USE_CASE_DEV_HEADSET, true) ||
                     pa_alsa_ucm_port_contains(p->name, SND_USE_CASE_DEV_HEADPHONES, true))
                pa_device_port_set_available(p, hp_avail(as->current_value));
/* ENDIF */
        }
        if (p->is_input) {
            if (!strcmp(p->name, "analog-input-headset-mic"))
                pa_device_port_set_available(p, hsmic_avail(as->current_value));
/* IFDEF HAVE_UCM ? */
            else if (pa_alsa_ucm_port_contains(p->name, SND_USE_CASE_DEV_HEADSET, false))
                pa_device_port_set_available(p, hsmic_avail(as->current_value));
/* ENDIF */
        }
    }
}

static void udev_cb(pa_mainloop_api *a, pa_io_event *e, int fd,
                    pa_io_event_flags_t events, void *userdata) {

    pa_alsa_extcon *u = userdata;
    struct udev_device *d = udev_monitor_receive_device(u->udev.monitor);
    struct udev_list_entry *entry;
    struct android_switch *as;
    const char *devpath, *state;

    if (!d) {
        pa_log("udev_monitor_receive_device failed.");
        pa_assert(a);
        a->io_free(u->udev.event);
        u->udev.event = NULL;
        return;
    }

    devpath = udev_device_get_devpath(d);
    if (!devpath) {
        pa_log("udev_device_get_devpath failed.");
        goto out;
    }
    pa_log_debug("Got uevent with devpath=%s", devpath);

    as = find_matching_switch(u, devpath);
    if (!as)
        goto out;

    entry = udev_list_entry_get_by_name(
            udev_device_get_properties_list_entry(d), "SWITCH_STATE");
    if (!entry) {
        pa_log("udev_list_entry_get_by_name failed to find 'SWITCH_STATE' entry.");
        goto out;
    }

    state = udev_list_entry_get_value(entry);
    if (!state) {
        pa_log("udev_list_entry_get_by_name failed.");
        goto out;
    }

    if (pa_atou(state, &as->current_value) < 0) {
        pa_log_warn("Switch '%s' has invalid value '%s'", as->name, state);
        goto out;
    }

    notify_ports(u, as);

out:
    udev_device_unref(d);
}

static bool init_udev(pa_alsa_extcon *u, pa_core *core) {

    int fd;

    u->udev.udev = udev_new();
    if (!u->udev.udev) {
        pa_log("udev_new failed.");
        return false;
    }

    u->udev.monitor = udev_monitor_new_from_netlink(u->udev.udev, "udev");
    if (!u->udev.monitor) {
        pa_log("udev_monitor_new_from_netlink failed.");
        return false;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(u->udev.monitor, EXTCON_NAME, NULL) < 0) {
        pa_log("udev_monitor_filter_add_match_subsystem_devtype failed.");
        return false;
    }

    if (udev_monitor_enable_receiving(u->udev.monitor) < 0) {
        pa_log("udev_monitor_enable_receiving failed.");
        return false;
    }

    fd = udev_monitor_get_fd(u->udev.monitor);
    if (fd < 0) {
        pa_log("udev_monitor_get_fd failed");
        return false;
    }

    pa_assert_se(u->udev.event = core->mainloop->io_new(core->mainloop, fd,
                 PA_IO_EVENT_INPUT, udev_cb, u));

    return true;
}

pa_alsa_extcon *pa_alsa_extcon_new(pa_core *core, pa_card *card) {

    pa_alsa_extcon *u = pa_xnew0(pa_alsa_extcon, 1);

    pa_assert(core);
    pa_assert(card);
    /* pa_log_error("pa_alsa_extcon_new start 2"); */
    u->card = card;
    u->h2w = android_switch_new("h2w");
    if (!u->h2w)
        goto fail;

    if (!init_udev(u, core))
        goto fail;

    notify_ports(u, u->h2w);
    /* pa_log_error("pa_alsa_extcon_new finish"); */
    return u;

fail:
    pa_alsa_extcon_free(u);
    /* pa_log_error("pa_alsa_extcon_new fail"); */
    return NULL;
}

void pa_alsa_extcon_free(pa_alsa_extcon *u) {

    pa_assert(u);

    if (u->udev.event)
        u->card->core->mainloop->io_free(u->udev.event);

    if (u->udev.monitor)
        udev_monitor_unref(u->udev.monitor);

    if (u->udev.udev)
        udev_unref(u->udev.udev);

    if (u->h2w)
        android_switch_free(u->h2w);

    pa_xfree(u);
}
