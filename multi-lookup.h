#include <unistd.h>

FILE *process_args(int, char**);


void *request_on_file(void *);
void *look(void *);

int try_queue_push(char *);
int try_queue_pop(char *);
int try_write_out(char *);

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

