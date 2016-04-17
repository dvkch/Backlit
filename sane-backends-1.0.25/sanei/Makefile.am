##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

AM_CPPFLAGS = -I. -I$(srcdir) -I$(top_builddir)/include \
 -I$(top_srcdir)/include

noinst_LTLIBRARIES = libsanei.la

libsanei_la_SOURCES = sanei_ab306.c sanei_constrain_value.c \
  sanei_init_debug.c sanei_net.c sanei_wire.c sanei_codec_ascii.c \
  sanei_codec_bin.c sanei_scsi.c sanei_config.c sanei_config2.c \
  sanei_pio.c sanei_pa4s2.c sanei_auth.c sanei_usb.c sanei_thread.c \
  sanei_pv8630.c sanei_pp.c sanei_lm983x.c sanei_access.c sanei_tcp.c \
  sanei_udp.c sanei_magic.c sanei_ir.c
if HAVE_JPEG
libsanei_la_SOURCES += sanei_jpeg.c
endif
EXTRA_DIST = linux_sg3_err.h os2_srb.h sanei_DomainOS.c sanei_DomainOS.h
