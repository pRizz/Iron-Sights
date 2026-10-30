#ifndef PORT_BUILD_TIME_H
#define PORT_BUILD_TIME_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Display a baked UTC instant in the player's local time zone. Metadata stays UTC. */
void portFormatBuildTime(char *output, size_t capacity, const char *maybeUtc);
#ifdef __cplusplus
}
#endif
#endif
