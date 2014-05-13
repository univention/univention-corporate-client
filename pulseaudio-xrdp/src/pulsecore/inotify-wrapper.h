#ifndef fooinotifywrapperhfoo
#define fooinotifywrapperhfoo

#include <pulsecore/core.h>

typedef struct pa_inotify pa_inotify;

typedef void (*pa_inotify_cb)(void *userdata);

pa_inotify *pa_inotify_start(const char *filename, void *userdata, pa_inotify_cb cb, pa_core *c);
void pa_inotify_stop(pa_inotify *i);

#endif
