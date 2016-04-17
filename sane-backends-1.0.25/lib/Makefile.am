##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.
AM_CPPFLAGS = -I. -I$(top_builddir)/include -I$(top_srcdir)/include

noinst_LTLIBRARIES = liblib.la libfelib.la

libfelib_la_SOURCES = getopt.c getopt1.c md5.c syslog.c vsyslog.c

liblib_la_SOURCES = alloca.c getenv.c inet_ntop.c inet_pton.c \
	isfdtype.c sigprocmask.c sleep.c snprintf.c strcasestr.c \
	strdup.c strndup.c strsep.c usleep.c
