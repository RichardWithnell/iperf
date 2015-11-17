/*
 * iperf, Copyright (c) 2014, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */
#include "iperf_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <netinet/tcp.h>
#include <time.h>

#include "iperf.h"
#include "iperf_api.h"
#include "units.h"
#include "iperf_locale.h"
#include "net.h"


static int run(struct iperf_test *test);


/**************************************************************************/
int
main2(int argc, char **argv)
{
    struct iperf_test *test;

    // XXX: Setting the process affinity requires root on most systems.
    //      Is this a feature we really need?
#ifdef TEST_PROC_AFFINITY
    /* didnt seem to work.... */
    /*
     * increasing the priority of the process to minimise packet generation
     * delay
     */
    int rc = setpriority(PRIO_PROCESS, 0, -15);

    if (rc < 0) {
        perror("setpriority:");
        fprintf(stderr, "setting priority to valid level\n");
        rc = setpriority(PRIO_PROCESS, 0, 0);
    }
    
    /* setting the affinity of the process  */
    cpu_set_t cpu_set;
    int affinity = -1;
    int ncores = 1;

    //sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);
    //if (errno)
    //    perror("couldn't get affinity:");

    //if ((ncores = sysconf(_SC_NPROCESSORS_CONF)) <= 0)
    //    err("sysconf: couldn't get _SC_NPROCESSORS_CONF");

    //CPU_ZERO(&cpu_set);
    //CPU_SET(affinity, &cpu_set);
    //if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
    //    err("couldn't change CPU affinity");
#endif

    test = iperf_new_test();
    if (!test){
        iperf_err(NULL, "create new test error - %s", iperf_strerror(i_errno));
        return 1;
    }
    iperf_defaults(test);	/* sets defaults */

    if (iperf_parse_arguments(test, argc, argv) < 0) {
        iperf_err(test, "parameter error - %s", iperf_strerror(i_errno));
        fprintf(stderr, "\n");
        usage_long();
        return 1;
    }

    if (run(test) < 0){
        iperf_err(test, "error - %s", iperf_strerror(i_errno));
        return 1;
    }

    iperf_free_test(test);

    return 0;
}


static jmp_buf sigend_jmp_buf;

static void
sigend_handler(int sig)
{
    longjmp(sigend_jmp_buf, 1);
}

/**************************************************************************/
static int
run(struct iperf_test *test)
{
    int consecutive_errors;


    switch (test->role) {
        case 's':
	    if (test->daemon) {
    		int rc = daemon(0, 0);
    		if (rc < 0) {
    		    i_errno = IEDAEMON;
    		    iperf_err(test, "error - %s", iperf_strerror(i_errno));
                return 1;
    		}
	    }
	    consecutive_errors = 0;
	    if (iperf_create_pidfile(test) < 0) {
    		i_errno = IEPIDFILE;
    		iperf_err(test, "error - %s", iperf_strerror(i_errno));
            return 1;
	    }
            for (;;) {
		if (iperf_run_server(test) < 0) {
		    iperf_err(test, "error - %s", iperf_strerror(i_errno));
                    fprintf(stderr, "\n");
		    ++consecutive_errors;
		    if (consecutive_errors >= 5) {
		        fprintf(stderr, "too many errors, exiting\n");
			break;
		    }
                } else
		    consecutive_errors = 0;
                iperf_reset_test(test);
                if (iperf_get_test_one_off(test))
                    break;
            }
	    iperf_delete_pidfile(test);
            break;
	case 'c':
	    if (iperf_run_client(test) < 0) {
		    iperf_err(test, "error - %s", iperf_strerror(i_errno));
            return 1;
        }
            break;
        default:
            usage();
            break;
    }

    //iperf_catch_sigend(SIG_DFL);

    return 0;
}

int
main(int argc, char **argv)
{
    struct timespec monotime;
    int iterations = 5;
    int i = 0;
    int j = 0;
    clock_gettime(CLOCK_MONOTONIC, &monotime);
    fprintf(stdout, "iperf start %lld.%.9ld\n",
                    (long long)monotime.tv_sec,
                    (long)monotime.tv_nsec);

    printf("ARGC: %d\n", argc);

    for(i = 0; i < argc; i++){
        if(!strcmp(argv[i], "-s")){
            iterations = 1;
        }
    }

    for (i = 0; i < iterations; i++){
        for(j = 0; j < argc-1; j++){
            if(!strcmp(argv[j], "-p")){
                printf("Port is: %d\n", atoi(argv[j+1]));
                sprintf(argv[j+1], "%d\0", atoi(argv[j+1])+1);
                printf("Port is now: %d\n", atoi(argv[j+1]));
            }
        }
        main2(argc, argv);
        sleep(2);
    }

    clock_gettime(CLOCK_MONOTONIC, &monotime);
    fprintf(stdout, "iperf exit %lld.%.9ld\n",
                    (long long)monotime.tv_sec,
                    (long)monotime.tv_nsec);

    return 0;

}
