
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "util.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"

#define MIN_RESOLVER_THREADS 2

// globals
FILE *ofile;
int THREAD_MAX;


int
request_on_file(char *fname)
{
    int success = 0;
    char hostname[SBUFSIZE];

    FILE *infile = fopen(fname, "r");
    if(!inputfp) {
        sprintf(errorstr, "Error Opening Input File: %s", argv[i]);
        perror(errorstr);
        exit(EXIT_FAILURE);
    }

    /* write lines to queue when possible
       until file completely processed, then return */
    while(fscanf(infile, INPUTFS, hostname) > 0) {
        try_queue_push(hostname);
    }

    fclose(inputfp);
    return success;
}

int finished(void)
{
    int fin = 0;

    // finish condition:
    // all request on file threads have returned,
    // i.e., all hostnames added to queue
    // && the queue is empty,
    // && ???

    return 0;
}


/* Safely get a hostname from queue, write
   it into buf.
   Return 0 on success, -1 if queue empty,
   1 if we know there is work todo but queue empty ???  */
int try_pop(char *buf)
{

    return 1;
}

/*
 */
int look(void)
{
    int success = 0;
    char hostbuf[SBUFSIZE];
    char   ipbuf[INET6_ADDRSTRLEN];
    char outline[SBUFSIZE + INET6_ADDRSTRLEN];

    while(!finished()) {
        // try acquire a host to process
        try_pop(hostbuf);

        /* Lookup hostname and get IP string */
        if(dnslookup(hostbuf, ipbuf, sizeof(ipbuf))
                == UTIL_FAILURE) {
            fprintf(stderr, "dnslookup error: %s\n", hostbuf);
            strncpy(ipbuf, "", sizeof(ipbuf));
        }

        // build output line
        sprintf(outline, "%s,%s\n", hostbuf, ipbuf);

        if(!try_write_out(outline)) {
            fprintf(stderr, "fail in writing to file\n");
        }

    }

    return success;
}


/* Safely add a line to queue */
int
try_queue_push(char *)
{
    int success = 0;

    return success;
}


/* Safely add line to ofile. */
int
try_write_out(char *line)
{
    int success = 0;

    return success;
}


/* Verify input arguments: that files exists.
   Return ready FILE* to output file.
*/
FILE *process_args(int argc, char **argv)
{
    /* Check Arguments */
    if(argc < MINARGS) {
        fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
        fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    return outputfp;
}


int
main(int argc, char* argv[])
{
    int *fill_success;
    int n_infiles; //number of input files

    // looks like a define constant for conformity...
    THREAD_MAX = nthreads(MIN_RESOLVER_THREADS); // this many work threads
    n_infiles = argc - 2; // there will be as many req threads as infiles

    /* verify inputs and get outfile ptr */
    ofile = process_args(argc, argv);

    /* array to track completion of infile processing  */
    fill_success = calloc(n_infiles, sizeof(int));

    /* Loop Through Input Files, launching a thread for each */
    for(i = 1; i < (argc-1); i++) {
        // launch:
        fill_success[i-1] = go request_on_file(argv[i]);
    }

}

free(fill_success);
fclose(ofile);
return EXIT_SUCCESS;
}

