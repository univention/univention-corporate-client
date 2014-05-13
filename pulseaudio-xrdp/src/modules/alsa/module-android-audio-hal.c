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

#include "module-android-audio-hal-symdef.h"

PA_MODULE_AUTHOR("David Henningsson");
PA_MODULE_DESCRIPTION("Android Audio HAL (Voice call helper)");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(TRUE);
PA_MODULE_USAGE("");

#include <stdbool.h>
#include <stddef.h>

#include <pulsecore/log.h>
#include <pulsecore/card.h>
#include <pulsecore/once.h>
#include <pulsecore/core-util.h>

#include <android/hardware/hardware.h>
#include <android/hardware/audio.h>

#include "alsa-mixer.h"

const hw_module_t *primary_audio_module = NULL;

typedef struct pa_android_audio_hal {
    struct audio_hw_device *dev;
    struct audio_stream_out *ostream;
    struct audio_stream_in *istream;
    int odevice; /* AUDIO_DEVICE_OUT_xxx */
    int idevice; /* AUDIO_DEVICE_IN_xxx */
} pa_android_audio_hal;

static pa_android_audio_hal* pa_android_audio_hal_new()
{
    int r;
    struct audio_hw_device *device;
    struct pa_android_audio_hal *hal;

    if (!primary_audio_module) {
        r = hw_get_module_by_class("audio", "primary", &primary_audio_module);
        if (r < 0 || primary_audio_module == NULL) {
            pa_log("hw_get_module_by_class failed with error %d", r);
            return NULL;
        }
        pa_log_debug("Opened Android module: '%s' - '%s', author: '%s'",
                     primary_audio_module->id, primary_audio_module->name,
                     primary_audio_module->author);
    }

    r = audio_hw_device_open(primary_audio_module, &device);
    if (r || device == NULL) {
        pa_log("audio_hw_device_open failed with error %d", r);
        return NULL;
    }
    pa_log_debug("Opened audio hw device");

    r = device->init_check(device);
    if (r) {
        pa_log("init_check failed with error %d", r);
        audio_hw_device_close(device);
        return NULL;
    }
    pa_log_debug("init_check succeeded");

    hal = pa_xnew0(struct pa_android_audio_hal, 1);
    hal->dev = device;
    return hal;
}

static void print_err(int err, const char *func)
{
    if (err)
        pa_log("%s returned error %d", func, err);
    else
        pa_log_debug("%s succeeded", func);
}

static void set_stream_device(struct audio_stream *stream, int device)
{
    char *s;
    int err;

    if (!stream->set_parameters) {
        pa_log_warn("no set_parameters callback");
        return;
    }

    s = pa_sprintf_malloc("routing=%d;", device);
    pa_log_debug("Calling set_parameters with '%s'", s);

    err = stream->set_parameters(stream, s);
    print_err(err < 0 ? err : 0, "set_parameters");

    pa_xfree(s);
}

static void start_voice_call(pa_android_audio_hal *hal)
{
    pa_assert(hal);
    pa_assert(hal->dev);

    if (!hal->dev->set_mode)
        pa_log_warn("no set_mode callback");
    else
        print_err(hal->dev->set_mode(hal->dev, AUDIO_MODE_IN_CALL), "set_mode");

    if (!hal->ostream) {
        if (!hal->dev->open_output_stream)
            pa_log_warn("no open_output_stream callback");
        else {
            struct audio_config config = { .sample_rate = 8000,
                .channel_mask = AUDIO_CHANNEL_OUT_MONO, .format = AUDIO_FORMAT_PCM_16_BIT };
            print_err(hal->dev->open_output_stream(hal->dev, 1, hal->odevice,
                AUDIO_OUTPUT_FLAG_PRIMARY,
                &config, &hal->ostream), "open_output_stream");
        }
    }

    if (hal->ostream)
        set_stream_device(&hal->ostream->common, hal->odevice);

    if (!hal->istream) {
        if (!hal->dev->open_input_stream)
            pa_log_warn("no open_input_stream callback");
        else {
            struct audio_config config = { .sample_rate = 8000,
                .channel_mask = AUDIO_CHANNEL_IN_MONO, .format = AUDIO_FORMAT_PCM_16_BIT };
            print_err(hal->dev->open_input_stream(hal->dev, 2, hal->idevice,
                  &config, &hal->istream), "open_input_stream");
        }
    }

    if (hal->istream)
        set_stream_device(&hal->istream->common, hal->idevice);

}

static void set_voice_call_volume(pa_android_audio_hal *hal, double v)
{
    if (!hal->dev->set_voice_volume)
        pa_log_warn("no set_voice_volume callback");
    else
        print_err(hal->dev->set_voice_volume(hal->dev, v), "set_voice_volume");
}

static void set_voice_mic_mute(pa_android_audio_hal *hal, bool mute)
{
    if (!hal->dev->set_mic_mute)
        pa_log_warn("no set_mic_mute callback");
    else
        print_err(hal->dev->set_mic_mute(hal->dev, mute), "set_mic_mute");
}

static void stop_voice_call(pa_android_audio_hal *hal)
{
    pa_assert(hal);
    pa_assert(hal->dev);

    if (!hal->dev->set_mode)
        pa_log_warn("no set_mode callback");
    else
        print_err(hal->dev->set_mode(hal->dev, AUDIO_MODE_NORMAL), "set_mode");

    if (hal->ostream) {
        set_stream_device(&hal->ostream->common, hal->odevice);
        if (!hal->dev->close_output_stream)
            pa_log_warn("no close_output_stream callback");
        else {
            pa_log_debug("Closing output device");
            hal->dev->close_output_stream(hal->dev, hal->ostream);
            hal->ostream = NULL;
        }
    }

    if (hal->istream) {
        set_stream_device(&hal->istream->common, hal->idevice);
        if (!hal->dev->close_input_stream)
            pa_log_warn("no close_input_stream callback");
        else {
            pa_log_debug("Closing input device");
            hal->dev->close_input_stream(hal->dev, hal->istream);
            hal->istream = NULL;
        }
    }

    if (hal->dev->set_parameters) {
        pa_log_debug("Setting mode to normal through set_parameters (Nexus 4 workaround)");
        print_err(hal->dev->set_parameters(hal->dev, "CALL_KEY=0;"), "device set_parameters");
    }
}

static void update_devices(pa_android_audio_hal *hal)
{
    if (hal->istream)
        set_stream_device(&hal->istream->common, hal->idevice);
    if (hal->ostream)
        set_stream_device(&hal->ostream->common, hal->odevice);
}

static void pa_android_audio_hal_free(pa_android_audio_hal *hal)
{
    if (hal->dev) {
        stop_voice_call(hal);
        audio_hw_device_close(hal->dev);
    }

    pa_xfree(hal);
/*    android_dlclose(primary_audio_module->dso);
    primary_audio_module = NULL; */
}

static bool card_has_voice_call(pa_card *c)
{
    pa_card_profile *p;
    void *state;

    PA_HASHMAP_FOREACH(p, c->profiles, state)
        if (pa_streq(p->name, SND_USE_CASE_VERB_VOICECALL))
            return true;

    return false;
}

static bool profile_in_voice_call(pa_card_profile *p)
{
    return p && pa_streq(p->name, SND_USE_CASE_VERB_VOICECALL);
}

struct userdata {
    pa_hook_slot
        *card_profile_before_slot,
        *card_profile_after_slot,
        *sink_port_slot,
        *source_port_slot;
    pa_android_audio_hal *hal;
    int savedidev, savedodev;
};

static struct userdata* userdata_instance = NULL; /* We need this for the volume callback */

static bool get_devices_for_card(struct userdata *u, pa_card *card)
{
    pa_sink *sink;
    pa_source *source;
    uint32_t state;
    int idev = 0, odev = 0;
    bool hasports = false;

    PA_IDXSET_FOREACH(sink, card->sinks, state) {
        const char *n;
        if (!sink->active_port)
            continue;
        n = sink->active_port->name;
        hasports = true;

        pa_log_debug("Current output port: %s", n);
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_EARPIECE, true))
            odev |= AUDIO_DEVICE_OUT_EARPIECE;
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_SPEAKER, true))
            odev |= AUDIO_DEVICE_OUT_SPEAKER;
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_HEADSET, true))
            odev |= AUDIO_DEVICE_OUT_WIRED_HEADSET;
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_HEADPHONES, true))
            odev |= AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }

    PA_IDXSET_FOREACH(source, card->sources, state) {
        const char *n;
        if (!source->active_port)
            continue;
        n = source->active_port->name;
        hasports = true;

        pa_log_debug("Current input port: %s", n);
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_HANDSET, false))
            idev |= (odev & AUDIO_DEVICE_OUT_SPEAKER) ? AUDIO_DEVICE_IN_BACK_MIC : AUDIO_DEVICE_IN_BUILTIN_MIC;
        if (pa_alsa_ucm_port_contains(n, SND_USE_CASE_DEV_HEADSET, false))
            idev |= AUDIO_DEVICE_IN_WIRED_HEADSET;
    }

    if (hasports) {
        pa_log_debug("Card %s - active android devices: 0x%x (output) and 0x%x (input)",
                     card->name, odev, idev);
        u->savedidev = idev;
        u->savedodev = odev;
    }
    if (u->hal) {
        u->hal->idevice = u->savedidev;
        u->hal->odevice = u->savedodev;
    }

    return hasports;
}

static void sink_set_volume_cb(pa_sink *s)
{
    struct userdata *u = userdata_instance;
    pa_volume_t v = pa_cvolume_avg(&s->real_volume);
    double d = (double) v / (double) PA_VOLUME_NORM; /* pa_sw_volume_to_linear(v); */
    pa_log_debug("Setting voice volume for sink '%s' to %f", s->name, d);
    if (u->hal)
        set_voice_call_volume(u->hal, d);
}

static void source_set_mute_cb(pa_source *s)
{
    struct userdata *u = userdata_instance;
    pa_log_debug("Setting voice mic mute for source '%s' to %d", s->name, (int) s->muted);
    if (u->hal)
       set_voice_mic_mute(u->hal, s->muted);
}

static void teardown_voice_call(struct userdata *u, pa_card *card)
{
    /* remove volume hook */
    {
        uint32_t state;
        pa_sink *sink;
        pa_source *source;

        PA_IDXSET_FOREACH(sink, card->sinks, state) {
            pa_sink_state_t s = pa_sink_get_state(sink);
            if (s == PA_SINK_RUNNING || s == PA_SINK_IDLE) {
                pa_log_debug("Suspending sink %s (tearing down voice call)", sink->name);
                pa_sink_suspend(sink, true, PA_SUSPEND_SESSION);
            }
/*            pa_log_error("Debug: sink %s, flags %d, set_volume %p", sink->name, sink->flags, sink->set_volume); */
            if (sink->set_volume == sink_set_volume_cb)
                sink->set_volume = NULL;
        }

        PA_IDXSET_FOREACH(source, card->sources, state) {
            pa_source_state_t s = pa_source_get_state(source);
            if (s == PA_SOURCE_RUNNING || s == PA_SOURCE_IDLE) {
                pa_log_debug("Suspending source %s (tearing down voice call)", source->name);
                pa_source_suspend(source, true, PA_SUSPEND_SESSION);
            }
/*            pa_log_error("Debug: sink %s, flags %d, set_volume %p", sink->name, sink->flags, sink->set_volume); */
            if (source->set_mute == source_set_mute_cb)
                source->set_mute = NULL;
        }
    }

    stop_voice_call(u->hal);
}

static bool setup_voice_call(struct userdata *u, pa_card *card)
{
    if (!u->hal) {
        u->hal = pa_android_audio_hal_new();
        if (!u->hal)
            return false;
    }

    get_devices_for_card(u, card);
    start_voice_call(u->hal);

    /* setup volume hook */
    {
        uint32_t state;
        pa_sink *sink;
        pa_source *source;

        PA_IDXSET_FOREACH(sink, card->sinks, state) {
/*            pa_log_debug("Debug: sink %s, flags %d, set_volume %p", sink->name, sink->flags, sink->set_volume); */
            if (!sink->set_volume)
                sink->set_volume = sink_set_volume_cb;
            sink_set_volume_cb(sink);
        }

        PA_IDXSET_FOREACH(source, card->sources, state) {
            if (!source->set_mute)
                source->set_mute = source_set_mute_cb;
            source_set_mute_cb(source);
        }

    }

    return true;
}


static pa_hook_result_t sink_port_hook_callback(pa_core *c, pa_sink *sink, struct userdata* u) {

    if (!sink->card)
        return PA_HOOK_OK;
    if (!card_has_voice_call(sink->card) || !profile_in_voice_call(sink->card->active_profile))
        return PA_HOOK_OK;

    get_devices_for_card(u, sink->card);
    if (u->hal)
        update_devices(u->hal);
    return PA_HOOK_OK;
}

static pa_hook_result_t source_port_hook_callback(pa_core *c, pa_source *source, struct userdata* u) {

    if (!source->card)
        return PA_HOOK_OK;
    if (!card_has_voice_call(source->card) || !profile_in_voice_call(source->card->active_profile))
        return PA_HOOK_OK;

    get_devices_for_card(u, source->card);
    if (u->hal)
        update_devices(u->hal);
    return PA_HOOK_OK;
}

static pa_hook_result_t card_profile_before_hook_callback(pa_core *c, pa_card_profile *profile, struct userdata *u) {

    if (!card_has_voice_call(profile->card))
        return PA_HOOK_OK;

    if (u->hal && !profile_in_voice_call(profile)) {
        teardown_voice_call(u, profile->card);
    }

    get_devices_for_card(u, profile->card); /* Save for later usage */
    return PA_HOOK_OK;
}


static pa_hook_result_t card_profile_after_hook_callback(pa_core *c, pa_card *card, struct userdata *u) {

    if (!card_has_voice_call(card))
        return PA_HOOK_OK;

    if (profile_in_voice_call(card->active_profile))
        setup_voice_call(u, card);

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {

    pa_card *card;
    uint32_t state;

    struct userdata *u = pa_xnew0(struct userdata, 1);

    pa_assert(userdata_instance == NULL);
    userdata_instance = u;

    u->card_profile_before_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_CHANGING],
        PA_HOOK_LATE+30, (pa_hook_cb_t) card_profile_before_hook_callback, u);
    u->card_profile_after_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_CHANGED],
        PA_HOOK_LATE+30, (pa_hook_cb_t) card_profile_after_hook_callback, u);
    u->sink_port_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PORT_CHANGED],
        PA_HOOK_LATE+30, (pa_hook_cb_t) sink_port_hook_callback, u);
    u->source_port_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PORT_CHANGED],
        PA_HOOK_LATE+30, (pa_hook_cb_t) source_port_hook_callback, u);

    PA_IDXSET_FOREACH(card, m->core->cards, state) {
        if (!card_has_voice_call(card))
            continue;

/* This isn't working, because there are no devices on N4 if we start up in "Voice Call" mode. */
/*        if (profile_in_voice_call(card->active_profile))
            setup_voice_call(u, card);
        else */
            get_devices_for_card(u, card); /* Save for later usage */
    }

    return 0;
}

void pa__done(pa_module*m) {

    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->card_profile_before_slot)
        pa_hook_slot_free(u->card_profile_before_slot);
    if (u->card_profile_after_slot)
        pa_hook_slot_free(u->card_profile_after_slot);
    if (u->sink_port_slot)
        pa_hook_slot_free(u->sink_port_slot);
    if (u->source_port_slot)
        pa_hook_slot_free(u->source_port_slot);

    if (u->hal)
        pa_android_audio_hal_free(u->hal);

    if (u == userdata_instance)
        userdata_instance = NULL;

    pa_xfree(u);
}
