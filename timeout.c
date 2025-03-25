/*
 * timeout - run a command and timeout after a period of time
 *
 * Copyright (c) 2004,2015,2023,2025 by Landon Curt Noll.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright, this permission notice and text
 * this comment, and the disclaimer below appear in all of the following:
 *
 *       supporting documentation
 *       source copies
 *       source works derived from this source
 *       binaries derived from this source or from derived source
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * chongo (Landon Curt Noll) /\oo/\
 *
 * http://www.isthe.com/chongo/index.html
 * https://github.com/lcn2
 *
 * Share and enjoy!  :-)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


/*
 * official version
 */
#define VERSION "1.2.1 2025-03-24"	    /* format: major.minor YYYY-MM-DD */


/* #define UNUSED to nothing if this gives you a syntax error */
#define UNUSED __attribute__((unused))	/* new ANSI arg not used attribute */

#if !defined(HZ)
# define HZ 100	/* guess */
#endif


/*
 * usage message
 */
static const char * const usage =
  "usage: %s [-h] [-V] [-n] seconds cmd [arg ...]\n"
        "\n"
        "    -h            print help message and exit\n"
        "    -V            print version string and exit\n"
        "\n"
        "    -n            noop - do nothing (def: do the tasks)\n"
        "\n"
	"    seconds       seconds until timeout (may be a float)\n"
	"    cmd           command to execute until timeout\n"
	"   [arg...]       optional args to the command\n"
        "\n"
	"Exit codes:\n"
	"    0         all OK\n"
	"    2         -h and help string printed or -V and version string printed\n"
	"    3         command line error\n"
	" >= 10        internal error\n"
        "\n"
        "%s version: %s\n";


/*
 * static declarations
 */
static char *program = NULL;	/* our name */
static char *prog = NULL;	/* basename of program */
static const char * const version = VERSION;
/**/
static pid_t pidofchild = -1;	/* process ID of the child command */
/**/
static void pr_usage(FILE *stream);
static void child(char **argv);
static void killchild(void);
static void sigchld(UNUSED int signum);


/*
 * pr_usage - print usage message
 *
 * given:
 *
 *    stream - print usage message on stream, NULL ==> stderr
 */
static void
pr_usage(FILE *stream)
{
    /*
     * NULL stream means stderr
     */
    if (stream == NULL) {
	stream = stderr;
    }

    /*
     * firewall - change program if NULL
     */
    if (program == NULL) {
	program = "((NULL))";
    }

    /*
     * firewall set name if NULL
     */
    if (prog == NULL) {
	prog = rindex(program, '/');
    }
    /* paranoia if no / is found */
    if (prog == NULL) {
	prog = program;
    } else {
	++prog;
    }

    /*
     * print usage message to stderr
     */
    fprintf(stream, usage, program, prog, version);
}


int
main(int argc, char *argv[])
{
    double timeout;		/* seconds until timeout */
    struct timeval timer;	/* single timer for timeout seconds */
    struct sigaction act;	/* sigchld signal action */
    int noop = 0;		/* 1 => -n was given, no op */
    int i;

    /*
     * parse args
     */
    program = argv[0];
    while ((i = getopt(argc, argv, ":hVn")) != -1) {
	switch (i) {

	case 'h':		    /* -h - print help message and exit */
	    pr_usage(stderr);
	    exit(2); /*ooo*/
	    /*NOTREACHED*/

	case 'V':		    /* -V - print version string and exit */
	    (void) printf("%s\n", version);
	    exit(3); /*ooo*/
	    /*NOTREACHED*/

	case 'n':		    /* -n - noop: do nothing */
	    noop = 1;
	    break;

	case ':':
            (void) fprintf(stderr, "%s: ERROR: requires an argument -- %c\n", program, optopt);
	    pr_usage(stderr);
	    exit(3); /*ooo*/
	    /*NOTREACHED*/

	case '?':
            (void) fprintf(stderr, "%s: ERROR: illegal option -- %c\n", program, optopt);
	    pr_usage(stderr);
	    exit(3); /*ooo*/
	    /*NOTREACHED*/

	default:
	    fprintf(stderr, "%s: ERROR: invalid -flag\n", program);
	    exit(3); /*ooo*/
	    /*NOTREACHED*/
	}
    }
    /* skip over command line options */
    argv += optind;
    argc -= optind;
    /* check the arg count */
    if (argc < 2) {
	fprintf(stderr, "%s: ERROR: expected at least 2 args, found: %d\n", program, argc);
	pr_usage(stderr);
	exit(3); /*ooo*/
	/*NOTREACHED*/
    }
    /**/
    timeout = strtod(argv[0], NULL);
    if (timeout <= 0.0) {
	fprintf(stderr, "%s: ERROR: timeout: %s must be > 0.0\n", program, argv[0]);
	exit(3); /*ooo*/
    }
    /* advance over ourname and timeout arg */
    --argc;
    ++argv;

    /*
     * setup to catch child exiting
     *
     * NOTE: We avoid catching child stops.  Also SIGCHLD does not
     *	     cause (most) system calls to return failure.
     */
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigchld;
    act.sa_flags = SA_NOCLDSTOP|SA_RESTART;
    if (sigaction(SIGCHLD, &act, NULL) < 0) {
	fprintf(stderr, "%s: ERROR: main: SIGCHLD sigaction failed: %s\n",
		program, strerror(errno));
	exit(10); /*coo*/
    }

    /*
     * do nothing more if -n
     */
    if (noop) {
	exit(0); /*ooo*/
    }

    /*
     * fork child command
     */
    pidofchild = fork();
    if (pidofchild < 0) {
	fprintf(stderr, "%s: ERROR: main: fork failed: %s\n",
		program, strerror(errno));
	exit(11);
    } else if (pidofchild == 0) {
	/* child code */
	child(argv);
	/*NOTREACHED*/
    }
    /* parent code */

    /*
     * force close of standard input and standard output
     *
     * NOTE: We keep stderr open in case of an error message.
     *
     * NOTE: We do not fclose because we do not want to fflush any
     *	     pending data.
     */
    close(1);	/* stdout */
    close(0);	/* stdin */

    /*
     * wait for the timeout period
     *
     * Set the timer to 1 HZ tick later than timeout seconds.
     *
     * NOTE: If the child exists early, then a SIGCHILD will be caught by
     *	     the sigchld function and we will exit that way.
     */
    timer.tv_sec = (int)(timeout);
    timer.tv_usec = (int)(1000000.0 *
         (timeout - (double)timer.tv_sec + (1.0/(double)HZ)));
    (void) select(0, NULL, NULL, NULL, &timer);

    /*
     * kill the child process if it exists
     */
    killchild();
    exit(12);
}


/*
 * child - code executed by the child command
 *
 * NOTE: Only called by the child and does not return.
 */
static void
child(char **argv)
{
    struct sigaction act;	/* default signal action */

    /*
     * disable sigchld handler to avoid recursive loops
     */
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    act.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &act, NULL) < 0) {
	fprintf(stderr, "%s: child pid: %d: SIGCHLD sigaction failed: %s\n",
		program, (int)getpid(), strerror(errno));
	exit(13);
    }

    /*
     * execute the child command
     */
    if (execvp(argv[0], argv) < 0) {
	fprintf(stderr, "%s: child pid: %d: exec of %s failed: %s\n",
		program, (int)getpid(), argv[0], strerror(errno));
	exit(14);
    }
    fprintf(stderr, "%s: child pid: %d: fall thru exec code!\n",
		    program, (int)getpid());
    exit(15);
}


/*
 * killchild - kill the child process if it exists
 *
 * This child process is first given a SIGINT (as if ^C keyboard interrupt
 * was performed).  After 2/HZ, if the child process still exists, then
 * a SIGTERM is given.  If after another 2/HZ the child process still exists,
 * the SIGKILL (uncatchable kill) is given.
 *
 * NOTE: This function does not return if could not kill the child process.
 *
 * NOTE: This function does nothing of the child process does not exist
 *	 or was never created.
 */
static void
killchild(void)
{
    struct timeval hz;		/* 2/HZ sleep time */

    /*
     * firewall - nothing if no child or pid is bogus
     */
    if (pidofchild <= 1) {
	return;
    }

    /*
     * interrupt the child process if it still exists
     */
    if (kill(pidofchild, 0) >= 0) {
	kill(pidofchild, SIGINT);
    } else {
	/* child is gone */
	return;
    }

    /*
     * a little bit (2/HZ seconds) for the child to finish
     */
    hz.tv_sec = 0;
    hz.tv_usec = (int)(1000000.0 * (2.0/(double)HZ));
    (void) select(0, NULL, NULL, NULL, &hz);

    /*
     * terminate the child process if it still exists
     */
    if (kill(pidofchild, 0) >= 0) {
	kill(pidofchild, SIGTERM);
    } else {
	/* child is gone */
	return;
    }

    /*
     * a little bit (2/HZ seconds) for the child to finish
     */
    hz.tv_sec = 0;
    hz.tv_usec = (int)(1000000.0 * (2.0/(double)HZ));
    (void) select(0, NULL, NULL, NULL, &hz);

    /*
     * kill the child process if it still exists
     */
    if (kill(pidofchild, 0) >= 0) {
	kill(pidofchild, SIGKILL);
    } else {
	/* child is gone */
	return;
    }

    /*
     * fatal if the process still exists
     */
    if (kill(pidofchild, 0) >= 0) {
	fprintf(stderr, "%s: process %d will not die\n",
			program, (int)pidofchild);
	exit(16);
    }
    return;
}


/*
 * sigchld - SIGCHLD - child exit signal handler
 *
 * NOTE: This function will exit / not return.
 */
static void
sigchld(UNUSED int signum)
{
    /*
     * firewall - nothing if no child or pid is bogus
     */
    if (pidofchild <= 1) {
	return;
    }

    /*
     * reap the child zombe (just in case)
     */
    (void) waitpid(pidofchild, NULL, WNOHANG);

    /*
     * terminate parent
     */
    exit(0); /*ooo*/
}
