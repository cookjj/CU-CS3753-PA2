#include <unistd.h>

static inline int num_processors(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}


int
nthreads(int min)
{
    int cores = num_processors();
    return cores > min ? cores : min;
}

