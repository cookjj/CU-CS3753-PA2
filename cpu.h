#include <unistd.h>

static inline int num_processors(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}


