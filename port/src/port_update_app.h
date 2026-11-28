#ifndef PORT_UPDATE_APP_H
#define PORT_UPDATE_APP_H
/* The updater's process glue (port_update_startup.c), shared with its
 * screens and worker (port_update_ui.c). Game thread only. */
#include "port_update.h"

enum PortUpdateLaunch {
    PORT_UPDATE_LAUNCH_NONE,
    PORT_UPDATE_LAUNCH_CHECK,    /* Notify: look for a newer build */
    PORT_UPDATE_LAUNCH_INSTALL,  /* Automatic or --update-now: install it */
};

/* This executable's paths and build; NULL when they cannot be found. */
const struct PortUpdateEnv *portUpdateAppEnv(void);
/* A line in the run's log and in Data/Updates/log.txt. */
void portUpdateAppNote(const char *fmt, ...);
/* Starts the build the files now hold with this run's arguments and ends
 * this one; returns only when that is impossible. */
void portUpdateAppRestart(void);
/* What portUpdateStartup decided the launch should run; *force is set for
 * --update-now, which ignores the throttle. */
int portUpdateAppLaunch(int *force);
/* [updates] mode= of midas.ini, or the default. */
int portUpdateAppIniMode(void);

#endif
