
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "queue.h"
#include "util.h"
#include "multi-lookup.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"

#define MIN_RESOLVER_THREADS 2

#define QUEUE_EMPTY 1001
#define COMPLETE    1002

// globals
FILE *ofile;
int THREAD_MAX;
int n_infiles;
int *fill_success;

pthread_t *req_tpool;
queue q;

pthread_mutex_t qm; // queue mutex
pthread_mutex_t fm; // fill success array mutex
int donei; // accumulator for fill success array index


/* Check saved return values from q request filling
   threads to see if they're all done. */
int
fill_complete(void)
{
    int i, val;
    for(i = 0; i < n_infiles; i++) {
        val = fill_success[i];
        if(val != COMPLETE)
            return 0;
    }

    return 1;
}


/* Go thru lines of a file getting them into the queue */
void *
request_on_file(void *fname)
{
    int err;
    char hostname[SBUFSIZE] = "";
    char errorstr[SBUFSIZE] = "";

    FILE *infile = fopen((char *)fname, "r");
    if(!infile) {
        sprintf(errorstr, "Error Opening Input File: %s", (char*)fname);
        perror(errorstr);
        exit(EXIT_FAILURE);
    }

    /* write lines to queue when possible
       until file completely processed, then return */
    while((fscanf(infile, INPUTFS, hostname) > 0)) {
        err = try_queue_push(hostname);
        if(err == 1) { // queue full
            usleep(random()%100);
            continue;
        } else if(err == -1) {
            pthread_exit((void*)EXIT_FAILURE);
        }
    }

    pthread_mutex_lock(&fm);
    fill_success[donei++] = COMPLETE;
    pthread_mutex_unlock(&fm);

    fclose(infile);
    pthread_exit((void*)COMPLETE);
}




/* Keep popping hostnames and resolv until no work left.
 */
void *
look(void *arg)
{
    arg = NULL;
    char hostbuf[SBUFSIZE] = "";
    char   ipbuf[INET6_ADDRSTRLEN] = "";
    char outline[SBUFSIZE + INET6_ADDRSTRLEN] = "";

    for(;;) {
        /* try acquire a host to process */
        if(try_queue_pop(hostbuf) == QUEUE_EMPTY) {
            /* If empty queue and all lines have been added, then we done. */
            if(fill_complete()) {
                return EXIT_SUCCESS;
            } else {
                continue;
            }
        }

        /* Lookup hostname and get IP string */
        if(dnslookup(hostbuf, ipbuf, sizeof(ipbuf))
                == UTIL_FAILURE) {
            fprintf(stderr, "dnslookup error: %s\n", hostbuf);
            strcpy(ipbuf, "");
        }

        // build output line
        sprintf(outline, "%s,%s\n", hostbuf, ipbuf);

        if(try_write_out(outline) != 0) {
            fprintf(stderr, "fail in writing to file\n");
        }
    }
    return EXIT_SUCCESS;
}



/* Safely get a hostname from queue, write it into buf.
   Returns: 0 for okay, or QUEUE_EMPTY in that case.
 */
int
try_queue_pop(char *buf)
{
    int ret;
    char *top;
    top = NULL;

    pthread_mutex_lock(&qm);
    if(queue_is_empty(&q)) {
        ret = QUEUE_EMPTY;
    } else {
        top = (char*)queue_pop(&q);
        if(top != NULL) {
            strcpy(buf, top);
            free(top);
        }
        ret = 0; /* good */
    }

    pthread_mutex_unlock(&qm);
    return ret;
}



/* Safely add a line to queue. Sleep 0-100us if Q full. */
/* Return 0 on success.
   1 on queue full,
  -1 catastrophic failure. */
int
try_queue_push(char *buf)
{
    int ret, n;
    char *str;
    ret = -1;

    // acquire locked access
    pthread_mutex_lock(&qm);
    if(queue_is_full(&q)) {
        ret = 1;
    } else {
        n = strlen(buf);
        str = (char*) malloc(n * sizeof(char));
        strncpy(str, buf, n);
        ret = 0; // note placement in case of failure
        if(queue_push(&q, str) == QUEUE_FAILURE) { // this should be a race case
            fprintf(stderr, "Q push failure\n");
            exit(-1);
            ret = -1;
        }
    }

    pthread_mutex_unlock(&qm);
    return ret;
}


/* Safely add line to ofile. */
/* Return -1 on problem, 0 otherwise. */
int
try_write_out(char *line)
{
    int success = 0;
    if(fprintf(ofile, line) < 0) {
        return -1;
    }
    fflush(ofile);
    return success;
}


/* Verify input arguments: that files exists.
   Return ready FILE* to output file.
*/
FILE *
process_args(int argc, char **argv)
{
    int i;
    FILE *inputfp;
    char errorstr[SBUFSIZE];

    /* Check Arguments */
    if(argc < MINARGS) {
        fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
        fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
        exit(EXIT_FAILURE);
    }

    /* Loop Through Input Files just to check they are readable */
    for(i = 1; i < (argc-1); i++) {
        /* Open Input File */
        inputfp = fopen(argv[i], "r");
        if(!inputfp) {
            sprintf(errorstr, "Error Opening Input File: %s", argv[i]);
            perror(errorstr);
            exit(EXIT_FAILURE);
        }
        fclose(inputfp);
    }

    /* Open Output File */
    FILE *outputfp = fopen(argv[(argc-1)], "w");
    if(!outputfp) {
        perror("Error Opening Output File");
        exit(EXIT_FAILURE);
    }

    return outputfp;
}


int
main(int argc, char* argv[])
{
    donei = 0;
    int i, err;
    pthread_t *res_tpool;
    pthread_attr_t pt_attr;

    // looks like a define constant for conformity...
    THREAD_MAX = nthreads(MIN_RESOLVER_THREADS); // this many work threads
    n_infiles = argc - 2; // there will be as many req threads as infiles

    /* verify inputs and get outfile ptr */
    ofile = process_args(argc, argv);

    req_tpool = (pthread_t *) malloc(n_infiles  * sizeof(pthread_t));
    res_tpool = (pthread_t *) malloc(THREAD_MAX * sizeof(pthread_t));
    pthread_attr_init(&pt_attr);


    /* array to track completion of infile processing  */
    fill_success = calloc(n_infiles, sizeof(int));

    queue_init(&q, QUEUEMAXSIZE);

    if(pthread_mutex_init(&qm, NULL) != 0) {
        fprintf(stderr, "\nqueue mutex init failed\n");
        return -1;
    }

    if(pthread_mutex_init(&fm, NULL) != 0) {
        fprintf(stderr, "\nfile mutex init failed\n");
        return -1;
    }

    /* start requester thread for each file */
    for(i = 1; i < (argc-1); i++) {
        err = pthread_create(&req_tpool[i-1], &pt_attr,
                             request_on_file, (void *)argv[i]);
        if(err != 0)
            fprintf(stderr, "\nfailed to create thread :[%s]", strerror(err));
    }
    /* start resolver thread for each core */
    for(i = 0; i < THREAD_MAX; i++) {
        err = pthread_create(&res_tpool[i], &pt_attr, look, NULL);
        if(err != 0)
            fprintf(stderr, "\nfailed to create thread :[%s]", strerror(err));
    }

    /* Rejoin main process upon completion */
    for(i = 0; i < n_infiles; i++) {
        pthread_join(req_tpool[i], NULL); // save return vals
    }
    for(i = 0; i < THREAD_MAX; i++)
        pthread_join(res_tpool[i], NULL);

    pthread_mutex_destroy(&qm);
    pthread_mutex_destroy(&fm);
    free(fill_success);
    queue_cleanup(&q);
    free(res_tpool);
    free(req_tpool);
    fclose(ofile);
    return EXIT_SUCCESS;
}

