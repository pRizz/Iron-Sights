#ifndef PORT_ABOUT_H
#define PORT_ABOUT_H
#ifdef __cplusplus
extern "C" {
#endif
void portAboutOpen(void);
/* Only baked GitHub HTTPS destinations are exposed to the native browser. */
int portAboutUrlAllowed(const char *url);
/* Opens a local folder in the system's file manager (FORK-053 Open Folder). */
void portOpenFolder(const char *path);
#ifdef __cplusplus
}
#endif
#endif
