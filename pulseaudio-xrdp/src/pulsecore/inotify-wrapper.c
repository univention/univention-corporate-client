#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/inotify.h>
#include <errno.h>

#include "inotify-wrapper.h"
#include <pulse/mainloop.h>
#include <pulsecore/core-util.h>
#include <pulsecore/core-error.h>

struct pa_inotify {
    char *filename;
    void *callback_data;
    pa_inotify_cb callback;
    int fd;
    pa_io_event *io_event;
    pa_core *core;
};


static void inotify_cb(
        pa_mainloop_api *a,
        pa_io_event *e,
        int fd,
        pa_io_event_flags_t events,
        void *userdata) {

    struct {
        struct inotify_event event;
        char name[NAME_MAX+1];
    } buf;

    pa_inotify *i = userdata;
    int pid_fd;

    pa_assert(i);

    if (pa_read(fd, &buf, sizeof(buf), NULL) < (int) sizeof(buf.event))
        pa_log_warn("inotify did not read a full event.");
    else
        pa_log_debug("inotify callback, event mask: 0x%x", (int) buf.event.mask);

    pid_fd = pa_open_cloexec(i->filename, O_RDONLY
#ifdef O_NOFOLLOW
                       |O_NOFOLLOW
#endif
                       , S_IRUSR);

    if (pid_fd < 0) {
        if (i->callback)
            i->callback(i->callback_data);
    } else
        pa_close(pid_fd);
}


pa_inotify *pa_inotify_start(const char *filename, void *userdata, pa_inotify_cb cb, pa_core *core) {

    pa_inotify *i = pa_xnew0(pa_inotify, 1);
    pa_assert(i);

    i->core = core;
    pa_core_ref(core);

    i->filename = pa_xstrdup(filename);
    i->callback_data = userdata;
    i->callback = cb;
    i->fd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK);

    if (i->fd < 0) {
        pa_log("inotify_init1() failed: %s", pa_cstrerror(errno));
        pa_inotify_stop(i);
        return NULL;
    }

    if (inotify_add_watch(i->fd, filename, IN_DELETE_SELF|IN_MOVE_SELF) < 0) {
        pa_log("inotify_add_watch() failed: %s", pa_cstrerror(errno));
        pa_inotify_stop(i);
        return NULL;
    }

    pa_assert_se(i->io_event = core->mainloop->io_new(core->mainloop, i->fd, PA_IO_EVENT_INPUT, inotify_cb, i));

    return i;
}


void pa_inotify_stop(pa_inotify *i) {

    pa_assert(i);

    if (i->io_event)
        i->core->mainloop->io_free(i->io_event);
    if (i->fd)
        pa_close(i->fd);
    pa_xfree(i->filename);
    if (i->core)
        pa_core_unref(i->core);
    pa_xfree(i);
}
