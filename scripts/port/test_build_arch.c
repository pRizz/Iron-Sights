/* ROM-free check (LP64-007 Task 2): portBuildArchitecture() names the
 * architecture this file was compiled for, in the words the crash reports'
 * `architecture` tag, the log and build-info.json use. */
#include <stdio.h>
#include <string.h>
#include "port_build_arch.h"

static const char *expectedArchitecture(void)
{
#if defined(__x86_64__)
    return sizeof(void *) == 8 ? "x86_64" : "(x86_64 with 4-byte pointers)";
#elif defined(__i386__)
    return sizeof(void *) == 4 ? "i686" : "(i386 with 8-byte pointers)";
#elif defined(__aarch64__)
    return sizeof(void *) == 8 ? "aarch64" : "(aarch64 with 4-byte pointers)";
#else
    return "(a host this check does not know)";
#endif
}

int main(void)
{
    // Arrange
    const char *expected = expectedArchitecture();

    // Act
    const char *actual = portBuildArchitecture();

    // Assert
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "test_build_arch: FAIL: portBuildArchitecture() is \"%s\", expected \"%s\" (%zu-byte pointers)\n",
                actual, expected, sizeof(void *));
        return 1;
    }
    return 0;
}
