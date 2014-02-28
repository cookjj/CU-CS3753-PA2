
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "util.h"
#include "cpu.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"

#define MIN_RESOLVER_THREADS 2

FILE *process_args(int, char**);

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
            break;
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
    FILE *ofp;
    int num_threads;

    num_threads = nthreads(MIN_RESOLVER_THREADS);

    /* verify inputs and get outfile ptr */
    ofp = process_args(argc, argv);

    /* Local Vars */
    FILE* inputfp = NULL;
    FILE* outputfp = NULL;
    char hostname[SBUFSIZE];
    char errorstr[SBUFSIZE];
    char firstipstr[INET6_ADDRSTRLEN];
    int i;


    /* Loop Through Input Files */
    for(i = 1; i < (argc-1); i++) {

        /* Open Input File */
        inputfp = fopen(argv[i], "r");
        if(!inputfp) {
            sprintf(errorstr, "Error Opening Input File: %s", argv[i]);
            perror(errorstr);
            break;
        }

        /* Read File and Process*/
        while(fscanf(inputfp, INPUTFS, hostname) > 0) {

            /* Lookup hostname and get IP string */
            if(dnslookup(hostname, firstipstr, sizeof(firstipstr))
                    == UTIL_FAILURE) {
                fprintf(stderr, "dnslookup error: %s\n", hostname);
                strncpy(firstipstr, "", sizeof(firstipstr));
            }

            /* Write to Output File */
            fprintf(outputfp, "%s,%s\n", hostname, firstipstr);
        }

        /* Close Input File */
        fclose(inputfp);
    }

    /* Close Output File */
    fclose(outputfp);

    return EXIT_SUCCESS;
}

