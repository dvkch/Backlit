/* sane - Scanner Access Now Easy.
   Copyright (C) 1998-2001 Yuri Dario
   Copyright (C) 2003-2004 Gerhard Jaeger (pthread/process support)
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   OS/2
   Helper functions for the OS/2 port (using threads instead of forked
   processes). Don't use them in the backends, they are used automatically by
   macros.

   Other OS:
   use this lib, if you intend to let run your reader function within its own
   task (thread or process). Depending on the OS and/or the configure settings
   pthread or fork is used to achieve this goal.
*/

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_OS2_H
# define INCL_DOSPROCESS
# include <os2.h>
#endif
#ifdef __BEOS__
# undef USE_PTHREAD /* force */
# include <kernel/OS.h>
#endif
#if !defined USE_PTHREAD && !defined HAVE_OS2_H && !defined __BEOS__
# include <sys/wait.h>
#endif
#if defined USE_PTHREAD
# include <pthread.h>
#endif

#define BACKEND_NAME sanei_thread      /**< name of this module for debugging */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_thread.h"

#ifndef _VAR_NOT_USED
# define _VAR_NOT_USED(x)	((x)=(x))
#endif

typedef struct {

	int         (*func)( void* );
	SANE_Status  status;
	void        *func_data;

} ThreadDataDef, *pThreadDataDef;

static ThreadDataDef td;

/** for init issues - here only for the debug output
 */
void
sanei_thread_init( void )
{
	DBG_INIT();

	memset( &td, 0, sizeof(ThreadDataDef));
	td.status = SANE_STATUS_GOOD;
}

SANE_Bool
sanei_thread_is_forked( void )
{
#if defined USE_PTHREAD || defined HAVE_OS2_H || defined __BEOS__
	return SANE_FALSE;
#else
	return SANE_TRUE;
#endif
}

/* Use this to mark a SANE_Pid as invaild instead of marking with -1.
 */
static void
sanei_thread_set_invalid( SANE_Pid *pid )
{

#ifdef WIN32
#ifdef WINPTHREAD_API
	*pid = 0;
#else
	pid->p = 0;
#endif
#else
	*pid = -1;
#endif
}

/* Return if PID is a valid PID or not. */
SANE_Bool
sanei_thread_is_invalid( SANE_Pid pid )
{
	SANE_Bool rc = SANE_FALSE;

#ifdef WIN32
#ifdef WINPTHREAD_API
	if (pid == 0)
#else
	if (pid.p == 0)
#endif
	    rc = SANE_TRUE;
#else
	if (pid == -1)
	    rc = SANE_TRUE;
#endif

	return rc;
}

/* pthread_t is not an integer on all platform.  Do our best to return
 * a PID-like value from structure.  On platforms were it is an integer,
 * return that.
 */
static long
sanei_thread_pid_to_long( SANE_Pid pid )
{
	int rc;

#ifdef WIN32
#ifdef WINPTHREAD_API
	rc = pid;
#else
	rc = pid.p;
#endif
#else
	rc = pid;
#endif

	return rc;
}

int
sanei_thread_kill( SANE_Pid pid )
{
	DBG(2, "sanei_thread_kill() will kill %ld\n",
	    sanei_thread_pid_to_long(pid));
#ifdef USE_PTHREAD
#if defined (__APPLE__) && defined (__MACH__)
	return pthread_kill((pthread_t)pid, SIGUSR2);
#else
	return pthread_cancel((pthread_t)pid);
#endif
#elif defined HAVE_OS2_H
	return DosKillThread(pid);
#else
	return kill( pid, SIGTERM );
#endif
}

#ifdef HAVE_OS2_H

static void
local_thread( void *arg )
{
	pThreadDataDef ltd = (pThreadDataDef)arg;

	DBG( 2, "thread started, calling func() now...\n" );
	ltd->status = ltd->func( ltd->func_data );

	DBG( 2, "func() done - status = %d\n", ltd->status );
	_endthread();
}

/*
 * starts a new thread or process
 * parameters:
 * star  address of reader function
 * args  pointer to scanner data structure
 *
 */
SANE_Pid
sanei_thread_begin( int (*func)(void *args), void* args )
{
	SANE_Pid pid;

	td.func      = func;
	td.func_data = args;

	pid = _beginthread( local_thread, NULL, 1024*1024, (void*)&td );
	if ( pid == -1 ) {
		DBG( 1, "_beginthread() failed\n" );
		return -1;
	}
   
	DBG( 2, "_beginthread() created thread %d\n", pid );
	return pid;
}

SANE_Pid
sanei_thread_waitpid( SANE_Pid pid, int *status )
{
  if (status)
    *status = 0;
  return pid; /* DosWaitThread( (TID*) &pid, DCWW_WAIT);*/
}

int
sanei_thread_sendsig( SANE_Pid pid, int sig )
{
	return 0;
}

#elif defined __BEOS__

static int32
local_thread( void *arg )
{
	pThreadDataDef ltd = (pThreadDataDef)arg;

	DBG( 2, "thread started, calling func() now...\n" );
	ltd->status = ltd->func( ltd->func_data );

	DBG( 2, "func() done - status = %d\n", ltd->status );
	return ltd->status;
}

/*
 * starts a new thread or process
 * parameters:
 * star  address of reader function
 * args  pointer to scanner data structure
 *
 */
SANE_Pid
sanei_thread_begin( int (*func)(void *args), void* args )
{
	SANE_Pid pid;

	td.func      = func;
	td.func_data = args;

	pid = spawn_thread( local_thread, "sane thread (yes they can be)", B_NORMAL_PRIORITY, (void*)&td );
	if ( pid < B_OK ) {
		DBG( 1, "spawn_thread() failed\n" );
		return -1;
	}
	if ( resume_thread(pid) < B_OK ) {
		DBG( 1, "resume_thread() failed\n" );
		return -1;
	}
   
	DBG( 2, "spawn_thread() created thread %d\n", pid );
	return pid;
}

SANE_Pid
sanei_thread_waitpid( SANE_Pid pid, int *status )
{
  int32 st;
  if ( wait_for_thread(pid, &st) < B_OK )
    return -1;
  if ( status )
    *status = (int)st;
  return pid;
}

int
sanei_thread_sendsig( SANE_Pid pid, int sig )
{
	if (sig == SIGKILL)
		sig = SIGKILLTHR;
	return kill(pid, sig);
}

#else /* HAVE_OS2_H, __BEOS__ */

#ifdef USE_PTHREAD

/* seems to be undefined in MacOS X */
#ifndef PTHREAD_CANCELED
# define PTHREAD_CANCELED ((void *) -1)
#endif

/**
 */
#if defined (__APPLE__) && defined (__MACH__)
static void
thread_exit_handler( int signo )
{
	DBG( 2, "signal(%i) caught, calling pthread_exit now...\n", signo );
	pthread_exit( PTHREAD_CANCELED );
}
#endif


static void*
local_thread( void *arg )
{
	static int     status;
	pThreadDataDef ltd = (pThreadDataDef)arg;

#if defined (__APPLE__) && defined (__MACH__)
	struct sigaction act;

	sigemptyset(&(act.sa_mask));
	act.sa_flags   = 0;
	act.sa_handler = thread_exit_handler;
	sigaction( SIGUSR2, &act, 0 );
#else
	int old;

	pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, &old );
	pthread_setcanceltype ( PTHREAD_CANCEL_ASYNCHRONOUS, &old );
#endif

	DBG( 2, "thread started, calling func() now...\n" );

	status = ltd->func( ltd->func_data );

	/* so sanei_thread_get_status() will work correctly... */
	ltd->status = status;

	DBG( 2, "func() done - status = %d\n", status );

	/* return the status, so pthread_join is able to get it*/
	pthread_exit((void*)&status );
}

/**
 */
static void
restore_sigpipe( void )
{
#ifdef SIGPIPE
	struct sigaction act;

	if( sigaction( SIGPIPE, NULL, &act ) == 0 ) {

		if( act.sa_handler == SIG_IGN ) {
			sigemptyset( &act.sa_mask );
			act.sa_flags   = 0;
			act.sa_handler = SIG_DFL;
			
			DBG( 2, "restoring SIGPIPE to SIG_DFL\n" );
			sigaction( SIGPIPE, &act, NULL );
		}
	}
#endif
}

#else /* the process stuff */

static int
eval_wp_result( SANE_Pid pid, int wpres, int pf )
{
	int retval = SANE_STATUS_IO_ERROR;

	if( wpres == pid ) {

		if( WIFEXITED(pf)) {
			retval = WEXITSTATUS(pf);
		} else {

			if( !WIFSIGNALED(pf)) {
				retval = SANE_STATUS_GOOD;
			} else {
				DBG( 1, "Child terminated by signal %d\n", WTERMSIG(pf));
				if( WTERMSIG(pf) == SIGTERM )
					retval = SANE_STATUS_GOOD;
			}
		}
	}
	return retval;
}
#endif

SANE_Pid
sanei_thread_begin( int (func)(void *args), void* args )
{
#ifdef USE_PTHREAD
	int result;
	pthread_t thread;
#ifdef SIGPIPE
	struct sigaction act;

	/* if signal handler for SIGPIPE is SIG_DFL, replace by SIG_IGN */
	if( sigaction( SIGPIPE, NULL, &act ) == 0 ) {

		if( act.sa_handler == SIG_DFL ) {
			sigemptyset( &act.sa_mask );
			act.sa_flags   = 0;
			act.sa_handler = SIG_IGN;

			DBG( 2, "setting SIGPIPE to SIG_IGN\n" );
			sigaction( SIGPIPE, &act, NULL );
		}
	}
#endif

	td.func      = func;
	td.func_data = args;

	result = pthread_create( &thread, NULL, local_thread, &td );
	usleep( 1 );

	if ( result != 0 ) {
		DBG( 1, "pthread_create() failed with %d\n", result );
		sanei_thread_set_invalid(&thread);
	}
	else
		DBG( 2, "pthread_create() created thread %ld\n",
		     (SANE_Pid)thread );

	return (SANE_Pid)thread;
#else
	SANE_Pid pid;
	pid = fork();
	if( pid < 0 ) {
		DBG( 1, "fork() failed\n" );
		return -1;
	}

	if( pid == 0 ) {

    	/* run in child context... */
		int status = func( args );
		
		/* don't use exit() since that would run the atexit() handlers */
		_exit( status );
	}

	/* parents return */
	return pid;
#endif
}

int
sanei_thread_sendsig( SANE_Pid pid, int sig )
{
	DBG(2, "sanei_thread_sendsig() %d to thread (id=%ld)\n", sig,
	    sanei_thread_pid_to_long(pid));
#ifdef USE_PTHREAD
	return pthread_kill( (pthread_t)pid, sig );
#else
	return kill( pid, sig );
#endif
}

SANE_Pid
sanei_thread_waitpid( SANE_Pid pid, int *status )
{
#ifdef USE_PTHREAD
	int *ls;
#else
	int ls;
#endif
	SANE_Pid result;
	int stat;

	stat = 0;

	DBG(2, "sanei_thread_waitpid() - %ld\n",
	    sanei_thread_pid_to_long(pid));
#ifdef USE_PTHREAD
	int rc;
	rc = pthread_join( (pthread_t)pid, (void*)&ls );

	if( 0 == rc ) {
		if( PTHREAD_CANCELED == ls ) {
			DBG(2, "* thread has been canceled!\n" );
			stat = SANE_STATUS_GOOD;
		} else {
			stat = *ls;
		}
		DBG(2, "* result = %d (%p)\n", stat, (void*)status );
		result = pid;
	}
	if ( EDEADLK == rc ) {
		if ( (pthread_t)pid != pthread_self() ) {
			/* call detach in any case to make sure that the thread resources 
			 * will be freed, when the thread has terminated
			 */
			DBG(2, "* detaching thread(%ld)\n", pid );
			pthread_detach((pthread_t)pid);
		}
	}
	if (status)
		*status = stat;

	restore_sigpipe();
#else
	result = waitpid( pid, &ls, 0 );
	if((result < 0) && (errno == ECHILD)) {
		stat   = SANE_STATUS_GOOD;
		result = pid;
	} else {
		stat = eval_wp_result( pid, result, ls );
		DBG(2, "* result = %d (%p)\n", stat, (void*)status );
	}
	if( status )
		*status = stat;
#endif
	return result;
}

#endif /* HAVE_OS2_H */

SANE_Status
sanei_thread_get_status( SANE_Pid pid )
{
#if defined USE_PTHREAD || defined HAVE_OS2_H || defined __BEOS__
	_VAR_NOT_USED( pid );

	return td.status;
#else
	int ls, stat, result;

	stat = SANE_STATUS_IO_ERROR;
	if( pid > 0 ) {

		result = waitpid( pid, &ls, WNOHANG );

		stat = eval_wp_result( pid, result, ls );
	}
	return stat;
#endif
}

/* END sanei_thread.c .......................................................*/
