##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

SUBDIRS = include lib sanei backend frontend tools doc po testsuite
DIST_SUBDIRS = include lib sanei backend frontend tools doc po japi testsuite

dist_doc_DATA = AUTHORS ChangeLog COPYING LICENSE NEWS PROBLEMS PROJECTS \
 README README.aix README.beos README.darwin README.djpeg README.freebsd \
 README.hp-ux README.linux README.netbsd README.openbsd README.os2 \
 README.solaris README.unixware2 README.unixware7 README.windows \
 README.zeta
EXTRA_DIST = ChangeLog-1.0.0 ChangeLog-1.0.1  ChangeLog-1.0.2 \
 ChangeLog-1.0.3 ChangeLog-1.0.4 ChangeLog-1.0.5 ChangeLog-1.0.6 \
 ChangeLog-1.0.7 ChangeLog-1.0.8 ChangeLog-1.0.9 ChangeLog-1.0.10 \
 ChangeLog-1.0.12 ChangeLog-1.0.13 ChangeLog-1.0.14 ChangeLog-1.0.15 \
 ChangeLog-1.0.16 ChangeLog-1.0.17 ChangeLog-1.0.18 ChangeLog-1.0.19 \
 ChangeLog-1.0.20 ChangeLog-1.0.21 ChangeLog-1.0.22 ChangeLog-1.0.23 \
 $(PACKAGE).lsm

#
# Keep the .cvsignore files sorted, and use this target to do it.
#
PERL=perl
sort-cvsignore:
	for f in `find . -name .cvsignore`; do \
	  $(PERL) -e 'print sort <>;' < $$f > $$f.tmptmp; \
	  mv $$f.tmptmp $$f; \
	done

#
# Check to make sure only sane_ and sanei_ symbols are exported from
# the libraries
#
libcheck:
	@echo "Libraries exporting 'illegal' symbols:"
	@for lib in backend/.libs/*.so backend/.libs/*.a; do \
	lines=`nm -g $$lib|grep '\( T \)\|\( D \)'|egrep -v ' _fini| _init'|egrep -v ' sane_| sanei_'`; \
	if test -n "$$lines" ; then \
	  echo -e "*** $$lib:\n$$lines"; \
	  found_errors="yes"; \
	fi \
	done; \
	echo; \
	echo "Libraries exporting 'non-standard sane_*' symbols:"; \
	for lib in backend/.libs/*.so backend/.libs/*.a; do \
	lines=`nm -g $$lib|grep '\( T \)\|\( D \)'|egrep ' sane_' | egrep -v 'sane_.*init|sane_.*exit|sane_.*get_devices|sane_.*open|sane_.*close|sane_.*get_option_descriptor|sane_.*control_option|sane_.*get_parameters|sane_.*start|sane_.*read|sane_.*cancel|sane_.*set_io_mode|sane_.*get_select_fd|sane_strstatus'` ; \
	if test -n "$$lines" ; then \
	  echo -e "*** $$lib:\n$$lines"; \
	  found_errors="yes"; \
	fi \
	done;
	if test ! -z $$found_errors ; then \
	  exit 1; \
	fi

# Verify that backends do not contain any unwanted public symbols
# anytime check or distcheck is ran.
# TODO: Currently this doesn't pass.  Uncomment once symbols have
# been cleaned up.
#check-am: libcheck

# Run testsuite any time installcheck or distcheck is ran.
# Must run testsuite after install or else shared library backends
# will not be found (including test backend).
installcheck-local:
	cd testsuite && $(MAKE) test.local

$(distdir).lsm: dist $(PACKAGE).lsm
	( cat $(PACKAGE).lsm | \
	  sed -e "s|_DATE_|`date +%d%b%y`|g" |\
	  sed -e "s|_VERSION_|$(VERSION)|g" | \
	  sed -e "s|_T_S_|`find $(distdir).tar.gz -printf "%4k"`|g" |\
	  sed -e "s|_L_S_|`find $(PACKAGE).lsm -printf "%4k"`|g" >\
	$(distdir).lsm \
	)
lsm: $(distdir).lsm

md5: dist
	md5sum $(distdir).tar.gz > $(distdir).tar.gz.md5

sane-backends: lsm md5

# Clean files created by custom targets in this file.
clean-local:
	rm -f sane-backends-*.lsm
	rm -f $(distdir).tar.gz.md5

# Clean files created by configure
distclean-local:
	rm -f include/_stdint.h
	rm -f include/byteorder.h
