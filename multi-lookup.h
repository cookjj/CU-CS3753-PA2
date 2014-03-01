#include <unistd.h>

FILE *process_args(int, char**);


int request_on_file(char *fname);
int look(void);

int try_queue_push(char *host);
int try_queue_pop(char *buf);
int try_write_out(char *line);

int finished(void);




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

