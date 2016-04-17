##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell, Olaf Meeuwissen, and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

INSTALL_LOCKPATH = @INSTALL_LOCKPATH@
LOCKPATH_GROUP = @LOCKPATH_GROUP@

BACKEND_LIBS_ENABLED=@BACKEND_LIBS_ENABLED@
BACKEND_CONFS_ENABLED=@BACKEND_CONFS_ENABLED@

DL_LIBS = @DL_LIBS@ 
LIBV4L_LIBS = @LIBV4L_LIBS@ 
MATH_LIB = @MATH_LIB@ 
IEEE1284_LIBS = @IEEE1284_LIBS@ 
TIFF_LIBS = @TIFF_LIBS@ 
JPEG_LIBS = @JPEG_LIBS@ 
GPHOTO2_LIBS = @GPHOTO2_LIBS@
GPHOTO2_LDFLAGS = @GPHOTO2_LDFLAGS@
SOCKET_LIBS = @SOCKET_LIBS@ 
AVAHI_LIBS = @AVAHI_LIBS@
USB_LIBS = @USB_LIBS@ 
SCSI_LIBS = @SCSI_LIBS@
PTHREAD_LIBS = @PTHREAD_LIBS@

AM_CPPFLAGS = -I. -I$(srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include -DLIBDIR="\"$(libdir)/sane\""

V_MAJOR = @V_MAJOR@
V_MINOR = @V_MINOR@
V_REV = @V_REV@

AM_LDFLAGS = @STRICT_LDFLAGS@
# The -rpath option is added because we are creating _LTLIBRARIES based
# on configure substitution.  This causes automake to not know the
# correct $libdir and must be added here.
DIST_SANELIBS_LDFLAGS = $(AM_LDFLAGS) -rpath '$(libdir)/sane' -version-number $(V_MAJOR):$(V_MINOR):$(V_REV) $(DYNAMIC_FLAG)
DIST_LIBS_LDFLAGS = $(AM_LDFLAGS) -rpath '$(libdir)' -version-number $(V_MAJOR):$(V_MINOR):$(V_REV)

# LIBTOOL install is a little to noisy for my liking.
LIBTOOL = @LIBTOOL@ --silent
FIRMWARE_DIRS = artec_eplus48u gt68xx snapscan epjitsu

EXTRA_DIST = sane_strstatus.c

all: becfg

EXTRA_DIST += stubs.c

# FIXME: % is a GNU extension... This is only thing left requiring
# use to use GNU make.
%-s.c: $(srcdir)/stubs.c
	rm -f $@
	$(LN_S) $(srcdir)/stubs.c $@

dll-preload.h:
	rm -f $@
	list="$(PRELOADABLE_BACKENDS)"; for be in $$list; do \
	  echo "PRELOAD_DECL($$be)" >> $@; \
	done
	echo "static struct backend preloaded_backends[] = {" >> $@
	sep=""; \
	list="$(PRELOADABLE_BACKENDS)"; \
	if test -z "$${list}"; then \
	  echo { 0, 0, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} >> $@; \
	else \
	  for be in $$list; do \
	    echo "$${sep}PRELOAD_DEFN($$be)" >> $@; \
	    sep=","; \
	  done; \
	fi
	echo "};" >> $@

# TODO: This really belongs together with the saned sources and
# should be installed there as well.
EXTRA_DIST += saned.conf.in

# Backends are not required to have a config file. Any backend
# that wants to install a config file should list it here.
BACKEND_CONFS= abaton.conf agfafocus.conf apple.conf artec.conf \
	       artec_eplus48u.conf avision.conf bh.conf \
	       canon630u.conf canon.conf canon_dr.conf \
	       canon_pp.conf cardscan.conf coolscan2.conf coolscan3.conf \
	       coolscan.conf dc210.conf dc240.conf dc25.conf \
	       dell1600n_net.conf dmc.conf epjitsu.conf epson2.conf \
	       epson.conf epsonds.conf fujitsu.conf genesys.conf gphoto2.conf \
	       gt68xx.conf hp3900.conf hp4200.conf hp5400.conf \
	       hp.conf hpsj5s.conf hs2p.conf ibm.conf kodak.conf kodakaio.conf\
	       leo.conf lexmark.conf ma1509.conf magicolor.conf \
	       matsushita.conf microtek2.conf microtek.conf mustek.conf \
	       mustek_pp.conf mustek_usb.conf nec.conf net.conf \
	       p5.conf \
	       pie.conf pieusb.conf pixma.conf plustek.conf plustek_pp.conf \
	       qcam.conf ricoh.conf rts8891.conf s9036.conf sceptre.conf \
	       sharp.conf sm3840.conf snapscan.conf sp15c.conf \
	       st400.conf stv680.conf tamarack.conf \
	       teco1.conf teco2.conf teco3.conf test.conf \
	       u12.conf umax1220u.conf umax.conf umax_pp.conf v4l.conf \
	       xerox_mfp.conf dll.conf saned.conf
becfg: $(BACKEND_CONFS)

SUFFIXES = .conf.in .conf
.conf.in.conf:
	@echo Generating $@ from $^
	@sed -e 's|@DATADIR@|$(datadir)|g' \
	     -e 's|@CONFIGDIR@|$(configdir)|g' \
	     -e 's|@DOCDIR@|$(docdir)|g' \
	     -e 's|@LIBDIR@|$(libdir)/sane|g' \
	     -e 's|@BINDIR@|$(bindir)|g' \
	     -e 's|@SBINDIR@|$(sbindir)|g' \
	     -e 's|@PACKAGEVERSION@|$(PACKAGE_VERSION)|g' $? > $@

install-data-hook: install-becfg install-firmware-path $(INSTALL_LOCKPATH)

# Custom install target to install config files.  Do not overwrite
# files that have been previously installed so that user modifications
# are not lost.
install-becfg:
	@# Libtool has a bug where it will sometimes symlink the last 
	@# installed library in $(sanelibdir) to $(sanelibdir)/libsane.*.  
	@# Having two libsane's can cause issues so get rid of it.
	-rm -f $(DESTDIR)$(sanelibdir)/libsane.*
	test -z "$(configdir)" || $(MKDIR_P) "$(DESTDIR)$(configdir)"
	test -z "$(configdir)/dll.d" || $(MKDIR_P) "$(DESTDIR)$(configdir)/dll.d"
	@list="$(BACKEND_CONFS_ENABLED) saned.conf dll.conf"; for cfg in $$list; do \
	  if test ! -r $${cfg}; then continue; fi; \
	  if test -f $(DESTDIR)$(configdir)/$${cfg}; then \
	  echo NOT overwriting $${cfg} in $(configdir)...; \
	  else \
	  echo installing $${cfg} in $(configdir)/$${cfg}...; \
	  $(INSTALL_DATA) $${cfg} $(DESTDIR)$(configdir)/$${cfg} \
	  || exit 1; \
	  fi; \
	  done

install-firmware-path:
	for dir in $(FIRMWARE_DIRS) ; do \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/sane/$${dir} ; \
	  done

install-lockpath:
	$(mkinstalldirs) -m 775 $(DESTDIR)$(locksanedir)

uninstall-hook:
	rm -rf $(DESTDIR)$(libdir)/sane $(DESTDIR)$(configdir) $(DESTDIR)$(locksanedir)
	rm -f $(DESTDIR)$(libdir)/libsane.*
	-for dir in $(FIRMWARE_DIRS) ; do \
	  rmdir $(DESTDIR)$(datadir)/sane/$${dir} ; \
	done

CLEANFILES = $(BACKEND_CONFS) $(be_convenience_libs)
clean-local:
	find . -type l -name \*-s.c | xargs rm -f


# Backends
#
# All possible backends should be listed here.  As a first step, we create
# a convenience library containing all files needed to link a backend
# directly into libsane.la.  Convenience library should have the
# form of lib${backend}.la to match what configure will list to
# build.
# Occasionally, this approach will have name conflicts with external
# libraries that need to be linked in.  See libgphoto2_i.la for
# example of working around that issue.
be_convenience_libs = libabaton.la libagfafocus.la \
    libapple.la libartec.la libartec_eplus48u.la \
    libas6e.la libavision.la libbh.la \
    libcanon.la libcanon630u.la libcanon_dr.la \
    libcanon_pp.la libcardscan.la libcoolscan.la \
    libcoolscan2.la libcoolscan3.la libdc25.la \
    libdc210.la libdc240.la libdell1600n_net.la \
    libdmc.la libdll.la libdll_preload.la libepjitsu.la libepson.la \
    libepson2.la libepsonds.la libfujitsu.la libgenesys.la \
    libgphoto2_i.la libgt68xx.la libhp.la \
    libhp3500.la libhp3900.la libhp4200.la \
    libhp5400.la libhp5590.la libhpljm1005.la \
    libhpsj5s.la libhs2p.la libibm.la libkodak.la libkodakaio.la\
    libkvs1025.la libkvs20xx.la libkvs40xx.la \
    libleo.la liblexmark.la libma1509.la libmagicolor.la \
    libmatsushita.la libmicrotek.la libmicrotek2.la \
    libmustek.la libmustek_pp.la libmustek_usb.la \
    libmustek_usb2.la libnec.la libnet.la \
    libniash.la libp5.la \
    libpie.la libpieusb.la libpint.la libpixma.la \
    libplustek.la libplustek_pp.la libpnm.la \
    libqcam.la libricoh.la librts8891.la \
    libs9036.la libsceptre.la libsharp.la \
    libsm3600.la libsm3840.la libsnapscan.la \
    libsp15c.la libst400.la libstv680.la \
    libtamarack.la libtest.la libteco1.la \
    libteco2.la libteco3.la libu12.la libumax.la \
    libumax1220u.la libumax_pp.la libv4l.la \
    libxerox_mfp.la

# Each stand alone backend thats possible to be built should be listed
# here.  There are the libraries that are installed under $(libdir)/sane.
# Format is libsane-${backend}.la.
be_dlopen_libs = libsane-abaton.la libsane-agfafocus.la \
    libsane-apple.la libsane-artec.la libsane-artec_eplus48u.la \
    libsane-as6e.la libsane-avision.la libsane-bh.la \
    libsane-canon.la libsane-canon630u.la libsane-canon_dr.la \
    libsane-canon_pp.la libsane-cardscan.la libsane-coolscan.la \
    libsane-coolscan2.la libsane-coolscan3.la libsane-dc25.la \
    libsane-dc210.la libsane-dc240.la libsane-dell1600n_net.la \
    libsane-dmc.la libsane-epjitsu.la libsane-epson.la \
    libsane-epson2.la libsane-epsonds.la libsane-fujitsu.la libsane-genesys.la \
    libsane-gphoto2.la libsane-gt68xx.la libsane-hp.la \
    libsane-hp3500.la libsane-hp3900.la libsane-hp4200.la \
    libsane-hp5400.la libsane-hp5590.la libsane-hpljm1005.la \
    libsane-hpsj5s.la libsane-hs2p.la libsane-ibm.la libsane-kodak.la libsane-kodakaio.la\
    libsane-kvs1025.la libsane-kvs20xx.la libsane-kvs40xx.la \
    libsane-leo.la \
    libsane-lexmark.la libsane-ma1509.la libsane-magicolor.la \
    libsane-matsushita.la libsane-microtek.la libsane-microtek2.la \
    libsane-mustek.la libsane-mustek_pp.la libsane-mustek_usb.la \
    libsane-mustek_usb2.la libsane-nec.la libsane-net.la \
    libsane-niash.la libsane-p5.la \
    libsane-pie.la libsane-pieusb.la libsane-pint.la libsane-pixma.la \
    libsane-plustek.la libsane-plustek_pp.la libsane-pnm.la \
    libsane-qcam.la libsane-ricoh.la libsane-rts8891.la \
    libsane-s9036.la libsane-sceptre.la libsane-sharp.la \
    libsane-sm3600.la libsane-sm3840.la libsane-snapscan.la \
    libsane-sp15c.la libsane-st400.la libsane-stv680.la \
    libsane-tamarack.la libsane-test.la libsane-teco1.la \
    libsane-teco2.la libsane-teco3.la libsane-u12.la libsane-umax.la \
    libsane-umax1220u.la libsane-umax_pp.la libsane-v4l.la \
    libsane-xerox_mfp.la

EXTRA_LTLIBRARIES = $(be_convenience_libs) $(be_dlopen_libs)

lib_LTLIBRARIES = libsane.la

sanelibdir = $(libdir)/sane
sanelib_LTLIBRARIES = $(BACKEND_LIBS_ENABLED) libsane-dll.la

COMMON_LIBS = ../lib/liblib.la

# Each backend should define a convenience library that compiles
# all related files within backend directory.  General guideline
# is to have a ${backend}.c and ${backend}.h.  Some backends also
# add a few support source files to convience library.
# Note: automake doesn't really use header files listed here. 
# They are indications that they need to be distributed only.
libabaton_la_SOURCES = abaton.c abaton.h
libabaton_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=abaton

# Each backend should define a stand alone library that gets installed.
# This will need to link in a special file ${backend}-s.c that allows
# the backend to be stand alone and contain all SANE API functions.
# Also, it will need to link in related convenience library as well as
# any external libraries required to resolve symbols.
#
# All backends should include $(DIST_SANELIBS_LDFLAGS) so that 
# library is correctly versioned.
#
# If a backend has a config file, it must be listed here to get distributed.
nodist_libsane_abaton_la_SOURCES = abaton-s.c
libsane_abaton_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=abaton
libsane_abaton_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_abaton_la_LIBADD = $(COMMON_LIBS) libabaton.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += abaton.conf.in

libagfafocus_la_SOURCES = agfafocus.c agfafocus.h
libagfafocus_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=agfafocus 

nodist_libsane_agfafocus_la_SOURCES = agfafocus-s.c
libsane_agfafocus_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=agfafocus 
libsane_agfafocus_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_agfafocus_la_LIBADD = $(COMMON_LIBS) libagfafocus.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += agfafocus.conf.in

libapple_la_SOURCES = apple.c apple.h
libapple_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=apple

nodist_libsane_apple_la_SOURCES = apple-s.c
libsane_apple_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=apple
libsane_apple_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_apple_la_LIBADD = $(COMMON_LIBS) libapple.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += apple.conf.in

libartec_la_SOURCES = artec.c artec.h
libartec_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=artec

nodist_libsane_artec_la_SOURCES = artec-s.c
libsane_artec_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_artec_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=artec
libsane_artec_la_LIBADD = $(COMMON_LIBS) libartec.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += artec.conf.in

libartec_eplus48u_la_SOURCES = artec_eplus48u.c artec_eplus48u.h
libartec_eplus48u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=artec_eplus48u

nodist_libsane_artec_eplus48u_la_SOURCES = artec_eplus48u-s.c
libsane_artec_eplus48u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=artec_eplus48u
libsane_artec_eplus48u_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_artec_eplus48u_la_LIBADD = $(COMMON_LIBS) libartec_eplus48u.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo $(MATH_LIB) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMEG_LIBS)
EXTRA_DIST += artec_eplus48u.conf.in

libas6e_la_SOURCES = as6e.c as6e.h
libas6e_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=as6e

nodist_libsane_as6e_la_SOURCES = as6e-s.c
libsane_as6e_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=as6e
libsane_as6e_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_as6e_la_LIBADD = $(COMMON_LIBS) libas6e.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo 

libavision_la_SOURCES = avision.c avision.h
libavision_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=avision

nodist_libsane_avision_la_SOURCES = avision-s.c 
libsane_avision_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=avision
libsane_avision_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_avision_la_LIBADD = $(COMMON_LIBS) libavision.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += avision.conf.in

libbh_la_SOURCES = bh.c bh.h
libbh_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=bh

nodist_libsane_bh_la_SOURCES = bh-s.c
libsane_bh_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=bh
libsane_bh_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_bh_la_LIBADD = $(COMMON_LIBS) libbh.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += bh.conf.in

libcanon_la_SOURCES = canon.c canon.h
libcanon_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon

nodist_libsane_canon_la_SOURCES = canon-s.c 
libsane_canon_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon
libsane_canon_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_canon_la_LIBADD = $(COMMON_LIBS) libcanon.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += canon.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += canon-sane.c canon-scsi.c

libcanon630u_la_SOURCES = canon630u.c
libcanon630u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon630u

nodist_libsane_canon630u_la_SOURCES = canon630u-s.c
libsane_canon630u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon630u
libsane_canon630u_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_canon630u_la_LIBADD = $(COMMON_LIBS) libcanon630u.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo  $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += canon630u.conf.in
# TODO: Why are this distributed but not compiled?
EXTRA_DIST += canon630u-common.c lm9830.h

libcanon_dr_la_SOURCES = canon_dr.c canon_dr.h canon_dr-cmd.h
libcanon_dr_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon_dr

nodist_libsane_canon_dr_la_SOURCES = canon_dr-s.c 
libsane_canon_dr_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon_dr
libsane_canon_dr_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_canon_dr_la_LIBADD = $(COMMON_LIBS) libcanon_dr.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += canon_dr.conf.in

libcanon_pp_la_SOURCES = canon_pp.c canon_pp.h canon_pp-io.c canon_pp-io.h canon_pp-dev.c canon_pp-dev.h
libcanon_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon_pp

nodist_libsane_canon_pp_la_SOURCES = canon_pp-s.c
libsane_canon_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=canon_pp
libsane_canon_pp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_canon_pp_la_LIBADD = $(COMMON_LIBS) libcanon_pp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(IEEE1284_LIBS)
EXTRA_DIST += canon_pp.conf.in

libcardscan_la_SOURCES = cardscan.c cardscan.h
libcardscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=cardscan

nodist_libsane_cardscan_la_SOURCES = cardscan-s.c
libsane_cardscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=cardscan
libsane_cardscan_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_cardscan_la_LIBADD = $(COMMON_LIBS) libcardscan.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += cardscan.conf.in

libcoolscan_la_SOURCES = coolscan.c coolscan.h coolscan-scsidef.h
libcoolscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan

nodist_libsane_coolscan_la_SOURCES = coolscan-s.c
libsane_coolscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan
libsane_coolscan_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_coolscan_la_LIBADD = $(COMMON_LIBS) libcoolscan.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_thread.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += coolscan.conf.in

libcoolscan2_la_SOURCES = coolscan2.c
libcoolscan2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan2

nodist_libsane_coolscan2_la_SOURCES = coolscan2-s.c
libsane_coolscan2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan2
libsane_coolscan2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_coolscan2_la_LIBADD = $(COMMON_LIBS) libcoolscan2.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += coolscan2.conf.in

libcoolscan3_la_SOURCES = coolscan3.c
libcoolscan3_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan3

nodist_libsane_coolscan3_la_SOURCES = coolscan3-s.c
libsane_coolscan3_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=coolscan3
libsane_coolscan3_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_coolscan3_la_LIBADD = $(COMMON_LIBS) libcoolscan3.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += coolscan3.conf.in

libdc25_la_SOURCES = dc25.c dc25.h
libdc25_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc25

nodist_libsane_dc25_la_SOURCES = dc25-s.c
libsane_dc25_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc25
libsane_dc25_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dc25_la_LIBADD = $(COMMON_LIBS) libdc25.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(MATH_LIB)
EXTRA_DIST += dc25.conf.in

libdc210_la_SOURCES = dc210.c dc210.h
libdc210_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc210

nodist_libsane_dc210_la_SOURCES = dc210-s.c
libsane_dc210_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc210
libsane_dc210_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dc210_la_LIBADD = $(COMMON_LIBS) libdc210.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo @SANEI_SANEI_JPEG_LO@ $(JPEG_LIBS)
EXTRA_DIST += dc210.conf.in

libdc240_la_SOURCES = dc240.c dc240.h
libdc240_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc240

nodist_libsane_dc240_la_SOURCES = dc240-s.c
libsane_dc240_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dc240
libsane_dc240_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dc240_la_LIBADD = $(COMMON_LIBS) libdc240.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo @SANEI_SANEI_JPEG_LO@ $(JPEG_LIBS)
EXTRA_DIST += dc240.conf.in

libdell1600n_net_la_SOURCES = dell1600n_net.c
libdell1600n_net_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dell1600n_net

nodist_libsane_dell1600n_net_la_SOURCES = dell1600n_net-s.c
libsane_dell1600n_net_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dell1600n_net
libsane_dell1600n_net_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dell1600n_net_la_LIBADD = $(COMMON_LIBS) libdell1600n_net.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(TIFF_LIBS) $(JPEG_LIBS) $(SOCKET_LIBS)
EXTRA_DIST += dell1600n_net.conf.in

libdmc_la_SOURCES = dmc.c dmc.h
libdmc_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dmc

nodist_libsane_dmc_la_SOURCES = dmc-s.c
libsane_dmc_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dmc
libsane_dmc_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dmc_la_LIBADD = $(COMMON_LIBS) libdmc.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += dmc.conf.in

libepjitsu_la_SOURCES = epjitsu.c epjitsu.h epjitsu-cmd.h
libepjitsu_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epjitsu

nodist_libsane_epjitsu_la_SOURCES = epjitsu-s.c
libsane_epjitsu_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epjitsu
libsane_epjitsu_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_epjitsu_la_LIBADD = $(COMMON_LIBS) libepjitsu.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += epjitsu.conf.in

libepson_la_SOURCES = epson.c epson.h epson_scsi.c epson_scsi.h epson_usb.c epson_usb.h
libepson_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epson

nodist_libsane_epson_la_SOURCES = epson-s.c
libsane_epson_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epson
libsane_epson_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_epson_la_LIBADD = $(COMMON_LIBS) libepson.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo  ../sanei/sanei_pio.lo $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += epson.conf.in

libepson2_la_SOURCES = epson2.c epson2.h epson2_scsi.c epson2_scsi.h epson2_usb.c epson2_net.c epson2_net.h epson2-io.c epson2-io.h epson2-commands.c epson2-commands.h epson2-ops.c epson2-ops.h epson2-cct.c
libepson2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epson2

nodist_libsane_epson2_la_SOURCES = epson2-s.c
libsane_epson2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epson2
libsane_epson2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_epson2_la_LIBADD = $(COMMON_LIBS) libepson2.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_pio.lo ../sanei/sanei_tcp.lo ../sanei/sanei_udp.lo $(SCSI_LIBS) $(USB_LIBS) $(SOCKET_LIBS) $(MATH_LIB) $(RESMGR_LIBS)
EXTRA_DIST += epson2.conf.in

libepsonds_la_SOURCES = epsonds.c epsonds.h epsonds-usb.c epsonds-usb.h epsonds-io.c epsonds-io.h \
		epsonds-cmd.c epsonds-cmd.h epsonds-ops.c epsonds-ops.h epsonds-jpeg.c epsonds-jpeg.h
libepsonds_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epsonds

nodist_libsane_epsonds_la_SOURCES = epsonds-s.c
libsane_epsonds_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=epsonds
libsane_epsonds_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_epsonds_la_LIBADD = $(COMMON_LIBS) libepsonds.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo \
				../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo \
				../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo \
				@SANEI_SANEI_JPEG_LO@ $(JPEG_LIBS) $(USB_LIBS) $(MATH_LIB) $(RESMGR_LIBS)
EXTRA_DIST += epsonds.conf.in

libfujitsu_la_SOURCES = fujitsu.c fujitsu.h fujitsu-scsi.h
libfujitsu_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=fujitsu

nodist_libsane_fujitsu_la_SOURCES = fujitsu-s.c
libsane_fujitsu_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=fujitsu
libsane_fujitsu_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_fujitsu_la_LIBADD = $(COMMON_LIBS) libfujitsu.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_magic.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += fujitsu.conf.in

libgenesys_la_SOURCES = genesys.c genesys.h genesys_gl646.c genesys_gl646.h genesys_gl841.c genesys_gl841.h genesys_gl843.c genesys_gl843.h genesys_gl846.c genesys_gl846.h genesys_gl847.c genesys_gl847.h genesys_gl124.c genesys_gl124.h genesys_low.c genesys_low.h
libgenesys_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=genesys

nodist_libsane_genesys_la_SOURCES = genesys-s.c
libsane_genesys_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=genesys
libsane_genesys_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_genesys_la_LIBADD = $(COMMON_LIBS) libgenesys.la  ../sanei/sanei_magic.lo ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += genesys.conf.in
# TODO: Why are this distributed but not compiled?
EXTRA_DIST += genesys_conv.c genesys_conv_hlp.c genesys_devices.c

libgphoto2_i_la_SOURCES = gphoto2.c gphoto2.h
libgphoto2_i_la_CPPFLAGS = $(AM_CPPFLAGS) @GPHOTO2_CPPFLAGS@ -DBACKEND_NAME=gphoto2

nodist_libsane_gphoto2_la_SOURCES = gphoto2-s.c 
libsane_gphoto2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=gphoto2
libsane_gphoto2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_gphoto2_la_LIBADD = $(GPHOTO2_LDFLAGS) $(COMMON_LIBS) libgphoto2_i.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo @SANEI_SANEI_JPEG_LO@ $(GPHOTO2_LIBS) $(JPEG_LIBS)
EXTRA_DIST += gphoto2.conf.in

libgt68xx_la_SOURCES = gt68xx.c gt68xx.h
libgt68xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=gt68xx

nodist_libsane_gt68xx_la_SOURCES = gt68xx-s.c 
libsane_gt68xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=gt68xx
libsane_gt68xx_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_gt68xx_la_LIBADD = $(COMMON_LIBS) libgt68xx.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += gt68xx.conf.in
# TODO: Why are this distributed but not compiled?
EXTRA_DIST += gt68xx_devices.c gt68xx_generic.c gt68xx_generic.h gt68xx_gt6801.c gt68xx_gt6801.h gt68xx_gt6816.c gt68xx_gt6816.h gt68xx_high.c gt68xx_high.h gt68xx_low.c gt68xx_low.h gt68xx_mid.c gt68xx_mid.h gt68xx_shm_channel.c gt68xx_shm_channel.h

libhp_la_SOURCES = hp.c hp.h hp-accessor.c hp-accessor.h hp-device.c hp-device.h hp-handle.c hp-handle.h hp-hpmem.c hp-option.c hp-option.h hp-scl.c hp-scl.h hp-scsi.h
libhp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp

nodist_libsane_hp_la_SOURCES = hp-s.c
libsane_hp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp
libsane_hp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp_la_LIBADD = $(COMMON_LIBS) libhp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_pio.lo ../sanei/sanei_thread.lo $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += hp.conf.in
# TODO: These should be moved to ../docs/hp; don't belong here.
EXTRA_DIST += hp.README hp.TODO

libhp3500_la_SOURCES = hp3500.c
libhp3500_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp3500

nodist_libsane_hp3500_la_SOURCES = hp3500-s.c
libsane_hp3500_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp3500
libsane_hp3500_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp3500_la_LIBADD = $(COMMON_LIBS) libhp3500.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)

libhp3900_la_SOURCES = hp3900.c
libhp3900_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp3900

nodist_libsane_hp3900_la_SOURCES = hp3900-s.c
libsane_hp3900_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp3900
libsane_hp3900_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp3900_la_LIBADD = $(COMMON_LIBS) libhp3900.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(TIFF_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += hp3900.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += hp3900_config.c hp3900_debug.c hp3900_rts8822.c hp3900_sane.c hp3900_types.c hp3900_usb.c

libhp4200_la_SOURCES = hp4200.c hp4200.h
libhp4200_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp4200

nodist_libsane_hp4200_la_SOURCES = hp4200-s.c 
libsane_hp4200_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp4200
libsane_hp4200_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp4200_la_LIBADD = $(COMMON_LIBS) libhp4200.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo  ../sanei/sanei_pv8630.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += hp4200.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += hp4200_lm9830.c hp4200_lm9830.h

libhp5400_la_SOURCES = hp5400.c hp5400.h
libhp5400_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp5400

nodist_libsane_hp5400_la_SOURCES = hp5400-s.c
libsane_hp5400_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp5400
libsane_hp5400_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp5400_la_LIBADD = $(COMMON_LIBS) libhp5400.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += hp5400.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += hp5400_debug.c hp5400_debug.h hp5400_internal.c hp5400_internal.h hp5400_sane.c hp5400_sanei.c hp5400_sanei.h hp5400_xfer.h

libhp5590_la_SOURCES = hp5590.c
libhp5590_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp5590

nodist_libsane_hp5590_la_SOURCES = hp5590-s.c
libsane_hp5590_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hp5590
libsane_hp5590_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hp5590_la_LIBADD = $(COMMON_LIBS) libhp5590.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += hp5590_cmds.c hp5590_cmds.h hp5590_low.c hp5590_low.h

libhpljm1005_la_SOURCES = hpljm1005.c
libhpljm1005_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hpljm1005

nodist_libsane_hpljm1005_la_SOURCES = hpljm1005-s.c
libsane_hpljm1005_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hpljm1005
libsane_hpljm1005_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hpljm1005_la_LIBADD = $(COMMON_LIBS) libhpljm1005.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)

libhpsj5s_la_SOURCES = hpsj5s.c hpsj5s.h
libhpsj5s_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hpsj5s

nodist_libsane_hpsj5s_la_SOURCES = hpsj5s-s.c 
libsane_hpsj5s_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hpsj5s
libsane_hpsj5s_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hpsj5s_la_LIBADD = $(COMMON_LIBS) libhpsj5s.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(IEEE1284_LIBS)
EXTRA_DIST += hpsj5s.conf.in

libhs2p_la_SOURCES = hs2p.c hs2p.h hs2p-saneopts.h
libhs2p_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hs2p

nodist_libsane_hs2p_la_SOURCES = hs2p-s.c 
libsane_hs2p_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=hs2p
libsane_hs2p_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_hs2p_la_LIBADD = $(COMMON_LIBS) libhs2p.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo  sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += hs2p.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += hs2p-scsi.c hs2p-scsi.h

libibm_la_SOURCES = ibm.c ibm.h
libibm_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ibm

nodist_libsane_ibm_la_SOURCES = ibm-s.c
libsane_ibm_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ibm
libsane_ibm_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_ibm_la_LIBADD = $(COMMON_LIBS) libibm.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += ibm.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += ibm-scsi.c

libkodak_la_SOURCES = kodak.c kodak.h kodak-cmd.h
libkodak_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kodak

nodist_libsane_kodak_la_SOURCES = kodak-s.c
libsane_kodak_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kodak
libsane_kodak_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_kodak_la_LIBADD = $(COMMON_LIBS) libkodak.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += kodak.conf.in

libkodakaio_la_SOURCES = kodakaio.c kodakaio.h
libkodakaio_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kodakaio

nodist_libsane_kodakaio_la_SOURCES = kodakaio-s.c
libsane_kodakaio_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kodakaio
libsane_kodakaio_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_kodakaio_la_LIBADD = $(COMMON_LIBS) libkodakaio.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo  ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_tcp.lo ../sanei/sanei_udp.lo  $(USB_LIBS) $(SOCKET_LIBS) $(AVAHI_LIBS) $(MATH_LIB) $(RESMGR_LIBS)
EXTRA_DIST += kodakaio.conf.in

libkvs1025_la_SOURCES = kvs1025.c kvs1025_low.c kvs1025_opt.c kvs1025_usb.c \
 kvs1025.h kvs1025_low.h kvs1025_usb.h kvs1025_cmds.h
libkvs1025_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs1025

nodist_libsane_kvs1025_la_SOURCES = kvs1025-s.c
libsane_kvs1025_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs1025
libsane_kvs1025_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_kvs1025_la_LIBADD = $(COMMON_LIBS) libkvs1025.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_magic.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)

libkvs20xx_la_SOURCES = kvs20xx.c kvs20xx_cmd.c kvs20xx_opt.c \
 kvs20xx_cmd.h kvs20xx.h 
libkvs20xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs20xx

nodist_libsane_kvs20xx_la_SOURCES = kvs20xx-s.c
libsane_kvs20xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs20xx
libsane_kvs20xx_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_kvs20xx_la_LIBADD = $(COMMON_LIBS) libkvs20xx.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS)

libkvs40xx_la_SOURCES = kvs40xx.c kvs40xx_cmd.c kvs40xx_opt.c \
 kvs40xx.h 
libkvs40xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs40xx

nodist_libsane_kvs40xx_la_SOURCES = kvs40xx-s.c
libsane_kvs40xx_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=kvs40xx
libsane_kvs40xx_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_kvs40xx_la_LIBADD = $(COMMON_LIBS) libkvs40xx.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)

libleo_la_SOURCES = leo.c leo.h
libleo_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=leo

nodist_libsane_leo_la_SOURCES = leo-s.c
libsane_leo_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=leo
libsane_leo_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_leo_la_LIBADD = $(COMMON_LIBS) libleo.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo  sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += leo.conf.in

liblexmark_la_SOURCES = lexmark.c lexmark.h lexmark_low.c
liblexmark_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=lexmark

nodist_libsane_lexmark_la_SOURCES = lexmark-s.c 
libsane_lexmark_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=lexmark
libsane_lexmark_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_lexmark_la_LIBADD = $(COMMON_LIBS) liblexmark.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += lexmark.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += lexmark_models.c lexmark_sensors.c

libma1509_la_SOURCES = ma1509.c ma1509.h
libma1509_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ma1509

nodist_libsane_ma1509_la_SOURCES = ma1509-s.c
libsane_ma1509_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ma1509
libsane_ma1509_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_ma1509_la_LIBADD = $(COMMON_LIBS) libma1509.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += ma1509.conf.in

libmagicolor_la_SOURCES = magicolor.c magicolor.h
libmagicolor_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=magicolor $(SNMP_CFLAGS)

nodist_libsane_magicolor_la_SOURCES = magicolor-s.c
libsane_magicolor_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=magicolor
libsane_magicolor_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_magicolor_la_LIBADD = $(COMMON_LIBS) libmagicolor.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo  ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_tcp.lo ../sanei/sanei_udp.lo  $(USB_LIBS) $(SOCKET_LIBS) $(MATH_LIB) $(RESMGR_LIBS) $(SNMP_LIBS)
EXTRA_DIST += magicolor.conf.in

libmatsushita_la_SOURCES = matsushita.c matsushita.h
libmatsushita_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=matsushita

nodist_libsane_matsushita_la_SOURCES = matsushita-s.c
libsane_matsushita_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=matsushita
libsane_matsushita_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_matsushita_la_LIBADD = $(COMMON_LIBS) libmatsushita.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo  sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += matsushita.conf.in

libmicrotek_la_SOURCES = microtek.c microtek.h
libmicrotek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=microtek

nodist_libsane_microtek_la_SOURCES = microtek-s.c
libsane_microtek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=microtek
libsane_microtek_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_microtek_la_LIBADD = $(COMMON_LIBS) libmicrotek.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += microtek.conf.in

libmicrotek2_la_SOURCES = microtek2.c microtek2.h
libmicrotek2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=microtek2

nodist_libsane_microtek2_la_SOURCES = microtek2-s.c
libsane_microtek2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=microtek2
libsane_microtek2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_microtek2_la_LIBADD = $(COMMON_LIBS) libmicrotek2.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo  sane_strstatus.lo ../sanei/sanei_scsi.lo  ../sanei/sanei_thread.lo $(MATH_LIB) $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += microtek2.conf.in

libmustek_la_SOURCES = mustek.c mustek.h
libmustek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek

nodist_libsane_mustek_la_SOURCES = mustek-s.c
libsane_mustek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek
libsane_mustek_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_mustek_la_LIBADD = $(COMMON_LIBS) libmustek.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo  ../sanei/sanei_thread.lo ../sanei/sanei_ab306.lo ../sanei/sanei_pa4s2.lo $(IEEE1284_LIBS) $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += mustek.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += mustek_scsi_pp.c mustek_scsi_pp.h

libmustek_pp_la_SOURCES = mustek_pp.c mustek_pp.h mustek_pp_decl.h mustek_pp_drivers.h
libmustek_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_pp

nodist_libsane_mustek_pp_la_SOURCES = mustek_pp-s.c
libsane_mustek_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_pp
libsane_mustek_pp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_mustek_pp_la_LIBADD = $(COMMON_LIBS) libmustek_pp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_pa4s2.lo $(MATH_LIB) $(IEEE1284_LIBS)
EXTRA_DIST += mustek_pp.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += mustek_pp_ccd300.c mustek_pp_ccd300.h mustek_pp_cis.c mustek_pp_cis.h mustek_pp_null.c 

libmustek_usb_la_SOURCES = mustek_usb.c mustek_usb.h
libmustek_usb_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_usb

nodist_libsane_mustek_usb_la_SOURCES = mustek_usb-s.c 
libsane_mustek_usb_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_usb
libsane_mustek_usb_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_mustek_usb_la_LIBADD = $(COMMON_LIBS) libmustek_usb.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += mustek_usb.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += mustek_usb_high.c mustek_usb_high.h mustek_usb_low.c mustek_usb_low.h mustek_usb_mid.c mustek_usb_mid.h

libmustek_usb2_la_SOURCES = mustek_usb2.c mustek_usb2.h
libmustek_usb2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_usb2

nodist_libsane_mustek_usb2_la_SOURCES = mustek_usb2-s.c
libsane_mustek_usb2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=mustek_usb2
libsane_mustek_usb2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_mustek_usb2_la_LIBADD = $(COMMON_LIBS) libmustek_usb2.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo $(MATH_LIB) $(PTHREAD_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += mustek_usb2_asic.c mustek_usb2_asic.h mustek_usb2_high.c mustek_usb2_high.h mustek_usb2_reflective.c mustek_usb2_transparent.c

libnec_la_SOURCES = nec.c nec.h
libnec_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=nec

nodist_libsane_nec_la_SOURCES = nec-s.c
libsane_nec_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=nec
libsane_nec_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_nec_la_LIBADD = $(COMMON_LIBS) libnec.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += nec.conf.in

libnet_la_SOURCES = net.c net.h
libnet_la_CPPFLAGS = $(AM_CPPFLAGS) @AVAHI_CFLAGS@ -DBACKEND_NAME=net

nodist_libsane_net_la_SOURCES = net-s.c
libsane_net_la_CPPFLAGS = $(AM_CPPFLAGS) @AVAHI_CFLAGS@ -DBACKEND_NAME=net
libsane_net_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_net_la_LIBADD = $(COMMON_LIBS) libnet.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_net.lo ../sanei/sanei_wire.lo ../sanei/sanei_codec_bin.lo $(AVAHI_LIBS) $(SOCKET_LIBS)
EXTRA_DIST += net.conf.in

libniash_la_SOURCES = niash.c
libniash_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=niash

nodist_libsane_niash_la_SOURCES = niash-s.c
libsane_niash_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=niash
libsane_niash_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_niash_la_LIBADD = $(COMMON_LIBS) libniash.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += niash_core.c niash_core.h niash_xfer.c niash_xfer.h

libpie_la_SOURCES = pie.c pie-scsidef.h
libpie_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pie

nodist_libsane_pie_la_SOURCES = pie-s.c 
libsane_pie_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pie
libsane_pie_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_pie_la_LIBADD = $(COMMON_LIBS) libpie.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo ../sanei/sanei_thread.lo $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += pie.conf.in

libpieusb_la_SOURCES = pieusb.h pieusb_buffer.c pieusb_buffer.h pieusb_scancmd.c pieusb_scancmd.h pieusb_specific.c pieusb_specific.h pieusb_usb.c pieusb_usb.h pieusb.c
libpieusb_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pieusb

nodist_libsane_pieusb_la_SOURCES = pieusb-s.c
libsane_pieusb_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pieusb
libsane_pieusb_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_pieusb_la_LIBADD = $(COMMON_LIBS) libpieusb.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo ../sanei/sanei_thread.lo ../sanei/sanei_usb.lo ../sanei/sanei_ir.lo ../sanei/sanei_magic.lo $(PTHREAD_LIBS) $(RESMGR_LIBS) $(USB_LIBS) $(MATH_LIB)
EXTRA_DIST += pieusb.conf.in

libp5_la_SOURCES = p5.c p5.h p5_device.h
libp5_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=p5

nodist_libsane_p5_la_SOURCES = p5-s.c
libsane_p5_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=p5
libsane_p5_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_p5_la_LIBADD = $(COMMON_LIBS) libp5.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo
EXTRA_DIST += p5.conf.in p5_device.c

libpint_la_SOURCES = pint.c pint.h
libpint_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pint

nodist_libsane_pint_la_SOURCES = pint-s.c
libsane_pint_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pint
libsane_pint_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_pint_la_LIBADD = $(COMMON_LIBS) libpint.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo 

libpixma_la_SOURCES = pixma.c pixma.h pixma_io_sanei.c pixma_io.h pixma_common.c pixma_common.h pixma_mp150.c pixma_mp730.c pixma_mp750.c pixma_mp810.c pixma_imageclass.c pixma_bjnp.c pixma_bjnp.h pixma_bjnp_private.h pixma_rename.h
libpixma_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pixma

nodist_libsane_pixma_la_SOURCES = pixma-s.c
libsane_pixma_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pixma
libsane_pixma_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_pixma_la_LIBADD = $(COMMON_LIBS) libpixma.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo $(MATH_LIB) $(SOCKET_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += pixma.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += pixma_sane_options.c pixma_sane_options.h

libplustek_la_SOURCES = plustek.c plustek.h
libplustek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=plustek

nodist_libsane_plustek_la_SOURCES = plustek-s.c
libsane_plustek_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=plustek
libsane_plustek_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_plustek_la_LIBADD = $(COMMON_LIBS) libplustek.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo ../sanei/sanei_lm983x.lo ../sanei/sanei_access.lo $(MATH_LIB) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += plustek.conf.in
EXTRA_DIST += plustek-usb.c plustek-usb.h plustek-usbcal.c plustek-usbcalfile.c plustek-usbdevs.c plustek-usbhw.c plustek-usbimg.c plustek-usbio.c plustek-usbmap.c plustek-usbscan.c plustek-usbshading.c

libplustek_pp_la_SOURCES = plustek_pp.c plustek-pp.h
libplustek_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=plustek_pp

nodist_libsane_plustek_pp_la_SOURCES = plustek_pp-s.c
libsane_plustek_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=plustek_pp
libsane_plustek_pp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_plustek_pp_la_LIBADD = $(COMMON_LIBS) libplustek_pp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_pp.lo ../sanei/sanei_thread.lo $(MATH_LIB) $(IEEE1284_LIBS) $(PTHREAD_LIBS)
EXTRA_DIST += plustek_pp.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += plustek-pp_dac.c plustek-pp_dbg.h plustek-pp_detect.c plustek-pp_genericio.c plustek-pp_hwdefs.h plustek-pp_image.c plustek-pp_io.c plustek-pp_map.c plustek-pp_misc.c plustek-pp_models.c plustek-pp_motor.c plustek-pp_p12.c plustek-pp_p12ccd.c plustek-pp_p48xx.c plustek-pp_p9636.c plustek-pp_procfs.c plustek-pp_procs.h plustek-pp_ptdrv.c plustek-pp_scale.c plustek-pp_scan.h plustek-pp_scandata.h plustek-pp_sysdep.h plustek-pp_tpa.c plustek-pp_types.h plustek-pp_wrapper.c

libpnm_la_SOURCES = pnm.c
libpnm_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pnm

nodist_libsane_pnm_la_SOURCES = pnm-s.c 
libsane_pnm_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=pnm
libsane_pnm_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_pnm_la_LIBADD = $(COMMON_LIBS) libpnm.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo 

libqcam_la_SOURCES = qcam.c qcam.h
libqcam_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=qcam

nodist_libsane_qcam_la_SOURCES = qcam-s.c
libsane_qcam_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=qcam
libsane_qcam_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_qcam_la_LIBADD = $(COMMON_LIBS) libqcam.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_pio.lo
EXTRA_DIST += qcam.conf.in

libricoh_la_SOURCES = ricoh.c ricoh.h
libricoh_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ricoh

nodist_libsane_ricoh_la_SOURCES = ricoh-s.c
libsane_ricoh_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=ricoh
libsane_ricoh_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_ricoh_la_LIBADD = $(COMMON_LIBS) libricoh.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += ricoh.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += ricoh-scsi.c

librts8891_la_SOURCES = rts8891.c rts8891.h rts88xx_lib.c rts88xx_lib.h
librts8891_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=rts8891

nodist_libsane_rts8891_la_SOURCES = rts8891-s.c
libsane_rts8891_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=rts8891
libsane_rts8891_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_rts8891_la_LIBADD = $(COMMON_LIBS) librts8891.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo ../sanei/sanei_scsi.lo  ../sanei/sanei_usb.lo $(SCSI_LIBS) $(USB_LIBS) $(RESMGR_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += rts8891.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += rts8891_devices.c rts8891_low.c rts8891_low.h

libs9036_la_SOURCES = s9036.c s9036.h
libs9036_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=s9036

nodist_libsane_s9036_la_SOURCES = s9036-s.c
libsane_s9036_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=s9036
libsane_s9036_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_s9036_la_LIBADD = $(COMMON_LIBS) libs9036.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += s9036.conf.in

libsceptre_la_SOURCES = sceptre.c sceptre.h
libsceptre_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sceptre

nodist_libsane_sceptre_la_SOURCES = sceptre-s.c
libsane_sceptre_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sceptre
libsane_sceptre_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_sceptre_la_LIBADD = $(COMMON_LIBS) libsceptre.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += sceptre.conf.in

libsharp_la_SOURCES = sharp.c sharp.h
libsharp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sharp

nodist_libsane_sharp_la_SOURCES = sharp-s.c
libsane_sharp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sharp
libsane_sharp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_sharp_la_LIBADD = $(COMMON_LIBS) libsharp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += sharp.conf.in

libsm3600_la_SOURCES = sm3600.c sm3600.h
libsm3600_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sm3600

nodist_libsane_sm3600_la_SOURCES = sm3600-s.c
libsane_sm3600_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sm3600
libsane_sm3600_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_sm3600_la_LIBADD = $(COMMON_LIBS) libsm3600.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += sm3600-color.c sm3600-gray.c sm3600-homerun.c sm3600-scanmtek.c sm3600-scantool.h sm3600-scanusb.c sm3600-scanutil.c

libsm3840_la_SOURCES = sm3840.c sm3840.h sm3840_params.h
libsm3840_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sm3840

nodist_libsane_sm3840_la_SOURCES = sm3840-s.c
libsane_sm3840_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sm3840
libsane_sm3840_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_sm3840_la_LIBADD = $(COMMON_LIBS) libsm3840.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += sm3840.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += sm3840_lib.c sm3840_lib.h sm3840_scan.c

libsnapscan_la_SOURCES = snapscan.c snapscan.h
libsnapscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=snapscan

nodist_libsane_snapscan_la_SOURCES = snapscan-s.c
libsane_snapscan_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=snapscan
libsane_snapscan_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_snapscan_la_LIBADD = $(COMMON_LIBS) libsnapscan.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += snapscan.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += snapscan-data.c snapscan-mutex.c snapscan-options.c snapscan-scsi.c snapscan-sources.c snapscan-sources.h snapscan-usb.c snapscan-usb.h

libsp15c_la_SOURCES = sp15c.c sp15c.h sp15c-scsi.h
libsp15c_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sp15c

nodist_libsane_sp15c_la_SOURCES = sp15c-s.c
libsane_sp15c_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=sp15c
libsane_sp15c_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_sp15c_la_LIBADD = $(COMMON_LIBS) libsp15c.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += sp15c.conf.in

libst400_la_SOURCES = st400.c st400.h
libst400_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=st400

nodist_libsane_st400_la_SOURCES = st400-s.c ../sanei/sanei_scsi.lo 
libsane_st400_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=st400
libsane_st400_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_st400_la_LIBADD = $(COMMON_LIBS) libst400.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo  sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += st400.conf.in

libstv680_la_SOURCES = stv680.c stv680.h
libstv680_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=stv680

nodist_libsane_stv680_la_SOURCES = stv680-s.c
libsane_stv680_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=stv680
libsane_stv680_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_stv680_la_LIBADD = $(COMMON_LIBS) libstv680.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += stv680.conf.in

libtamarack_la_SOURCES = tamarack.c tamarack.h
libtamarack_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=tamarack

nodist_libsane_tamarack_la_SOURCES = tamarack-s.c
libsane_tamarack_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=tamarack
libsane_tamarack_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_tamarack_la_LIBADD = $(COMMON_LIBS) libtamarack.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += tamarack.conf.in

libtest_la_SOURCES = test.c test.h
libtest_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=test

nodist_libsane_test_la_SOURCES = test-s.c
libsane_test_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=test
libsane_test_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_test_la_LIBADD = $(COMMON_LIBS) libtest.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_thread.lo  $(PTHREAD_LIBS)
EXTRA_DIST += test.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += test-picture.c

libteco1_la_SOURCES = teco1.c teco1.h
libteco1_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco1

nodist_libsane_teco1_la_SOURCES = teco1-s.c
libsane_teco1_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco1
libsane_teco1_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_teco1_la_LIBADD = $(COMMON_LIBS) libteco1.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += teco1.conf.in

libteco2_la_SOURCES = teco2.c teco2.h
libteco2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco2

nodist_libsane_teco2_la_SOURCES = teco2-s.c
libsane_teco2_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco2
libsane_teco2_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_teco2_la_LIBADD = $(COMMON_LIBS) libteco2.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += teco2.conf.in

libteco3_la_SOURCES = teco3.c teco3.h
libteco3_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco3

nodist_libsane_teco3_la_SOURCES = teco3-s.c
libsane_teco3_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=teco3
libsane_teco3_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_teco3_la_LIBADD = $(COMMON_LIBS) libteco3.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_scsi.lo $(SCSI_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += teco3.conf.in

libu12_la_SOURCES = u12.c u12.h
libu12_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=u12

nodist_libsane_u12_la_SOURCES = u12-s.c
libsane_u12_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=u12
libsane_u12_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_u12_la_LIBADD = $(COMMON_LIBS) libu12.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo $(MATH_LIB) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += u12.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += u12-ccd.c u12-hw.c u12-hwdef.h u12-if.c u12-image.c u12-io.c u12-map.c u12-motor.c u12-scanner.h u12-shading.c u12-tpa.c

libumax_la_SOURCES = umax.c umax.h
libumax_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax

nodist_libsane_umax_la_SOURCES = umax-s.c
libsane_umax_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax
libsane_umax_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_umax_la_LIBADD = $(COMMON_LIBS) libumax.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_thread.lo ../sanei/sanei_scsi.lo ../sanei/sanei_pv8630.lo $(MATH_LIB) $(SCSI_LIBS) $(USB_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += umax.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += umax-scanner.c umax-scanner.h umax-scsidef.h umax-uc1200s.c umax-uc1200se.c umax-uc1260.c umax-uc630.c umax-uc840.c umax-ug630.c umax-ug80.c umax-usb.c

libumax1220u_la_SOURCES = umax1220u.c
libumax1220u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax1220u

nodist_libsane_umax1220u_la_SOURCES = umax1220u-s.c 
libsane_umax1220u_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax1220u
libsane_umax1220u_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_umax1220u_la_LIBADD = $(COMMON_LIBS) libumax1220u.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_pv8630.lo $(MATH_LIB) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += umax1220u.conf.in
# TODO: Why are these distributed but not compiled?
EXTRA_DIST += umax1220u-common.c

libumax_pp_la_SOURCES = umax_pp.c umax_pp.h umax_pp_low.c umax_pp_low.h umax_pp_mid.c umax_pp_mid.h
libumax_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax_pp

nodist_libsane_umax_pp_la_SOURCES = umax_pp-s.c
libsane_umax_pp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=umax_pp
libsane_umax_pp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_umax_pp_la_LIBADD = $(COMMON_LIBS) libumax_pp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(MATH_LIB)
EXTRA_DIST += umax_pp.conf.in

libv4l_la_SOURCES = v4l.c v4l.h v4l-frequencies.h
libv4l_la_CPPFLAGS = $(AM_CPPFLAGS) @LIBV4L_CFLAGS@ -DBACKEND_NAME=v4l

nodist_libsane_v4l_la_SOURCES = v4l-s.c
libsane_v4l_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=v4l
libsane_v4l_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_v4l_la_LIBADD = $(COMMON_LIBS) libv4l.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo $(LIBV4L_LIBS)
EXTRA_DIST += v4l.conf.in

libxerox_mfp_la_SOURCES = xerox_mfp.c xerox_mfp-usb.c xerox_mfp-tcp.c xerox_mfp.h
libxerox_mfp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=xerox_mfp

nodist_libsane_xerox_mfp_la_SOURCES = xerox_mfp-s.c
libsane_xerox_mfp_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=xerox_mfp
libsane_xerox_mfp_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_xerox_mfp_la_LIBADD = $(COMMON_LIBS) libxerox_mfp.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo  sane_strstatus.lo ../sanei/sanei_usb.lo ../sanei/sanei_tcp.lo $(MATH_LIB) $(SOCKET_LIBS) $(USB_LIBS) $(RESMGR_LIBS)
EXTRA_DIST += xerox_mfp.conf.in

libdll_preload_la_SOURCES =  dll.c
libdll_preload_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dll -DENABLE_PRELOAD
libdll_la_SOURCES =  dll.c
libdll_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dll
BUILT_SOURCES = dll-preload.h
CLEANFILES += dll-preload.h

nodist_libsane_dll_la_SOURCES =  dll-s.c
libsane_dll_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dll
libsane_dll_la_LDFLAGS = $(DIST_SANELIBS_LDFLAGS)
libsane_dll_la_LIBADD = $(COMMON_LIBS) libdll.la ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo sane_strstatus.lo $(DL_LIBS)
EXTRA_DIST += dll.conf.in
# TODO: Why is this distributed but not installed?
EXTRA_DIST += dll.aliases

# libsane.la and libsane-dll.la are the same thing except for
# the addition of backends listed by PRELOADABLE_BACKENDS that are 
# statically linked in.
# Also, libsane.la goes into $(libdir) where as all libsane-*
# (including libsane-dll.la) go into $(libdir)/sane

# FIXME: Since we are throwing in the kitchen sink, might as
# well link in ../sanei/libsanei.la instead.  But currently,
# libsanei.la is linking in sanei_auth which requires md5. 
# Shipping md5 could cause symbol conflicts with commonly used
# md5 external libraries.  Either need to prefix md5 with sanei_
# (see liblib.la and snprintf), or move sanei_auth outside
# of libsanei.
#
# FIXME: This is linking in every possible external library because there
# is the off chance user is using PRELOADABLE_BACKENDS that may need
# them. Since standard mode is to only have the dll backend, its a waste.
# Need to update configure to build a list of only what needs to go into
# LIBADD based on whats being preloaded.
nodist_libsane_la_SOURCES =  dll-s.c
libsane_la_CPPFLAGS = $(AM_CPPFLAGS) -DBACKEND_NAME=dll
libsane_la_LDFLAGS = $(DIST_LIBS_LDFLAGS)
libsane_la_LIBADD = $(COMMON_LIBS) @PRELOADABLE_BACKENDS_ENABLED@ libdll_preload.la sane_strstatus.lo ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_pv8630.lo ../sanei/sanei_pp.lo ../sanei/sanei_thread.lo  ../sanei/sanei_lm983x.lo ../sanei/sanei_access.lo ../sanei/sanei_net.lo ../sanei/sanei_wire.lo ../sanei/sanei_codec_bin.lo ../sanei/sanei_pa4s2.lo ../sanei/sanei_ab306.lo ../sanei/sanei_pio.lo ../sanei/sanei_tcp.lo ../sanei/sanei_udp.lo ../sanei/sanei_magic.lo $(DL_LIBS) $(LIBV4L_LIBS) $(MATH_LIB) $(IEEE1284_LIBS) $(TIFF_LIBS) $(JPEG_LIBS) $(GPHOTO2_LIBS) $(SOCKET_LIBS) $(USB_LIBS) $(AVAHI_LIBS) $(SCSI_LIBS) $(PTHREAD_LIBS) $(RESMGR_LIBS)

# WARNING: Automake is getting this wrong so have to do it ourselves.
libsane_la_DEPENDENCIES = $(COMMON_LIBS) @PRELOADABLE_BACKENDS_ENABLED@ libdll_preload.la sane_strstatus.lo ../sanei/sanei_init_debug.lo ../sanei/sanei_constrain_value.lo ../sanei/sanei_config.lo ../sanei/sanei_config2.lo ../sanei/sanei_usb.lo ../sanei/sanei_scsi.lo ../sanei/sanei_pv8630.lo ../sanei/sanei_pp.lo ../sanei/sanei_thread.lo  ../sanei/sanei_lm983x.lo ../sanei/sanei_access.lo ../sanei/sanei_net.lo ../sanei/sanei_wire.lo ../sanei/sanei_codec_bin.lo ../sanei/sanei_pa4s2.lo ../sanei/sanei_ab306.lo ../sanei/sanei_pio.lo ../sanei/sanei_tcp.lo ../sanei/sanei_udp.lo ../sanei/sanei_magic.lo @SANEI_SANEI_JPEG_LO@
