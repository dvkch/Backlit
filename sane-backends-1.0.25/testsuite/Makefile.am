##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

SUBDIRS = sanei tools

SCANIMAGE = ../frontend/scanimage$(EXEEXT)
TESTFILE  = $(srcdir)/testfile.pnm
OUTFILE   = outfile.pnm
DEVICE    = test
OPTIONS   = --mode Color --depth 16 --test-picture "Color pattern" --resolution 50 -y 20 -x 20 > $(OUTFILE)

EXTRA_DIST = README testfile.pnm
CLEANFILES = $(OUTFILE)

all: help

help:
	@echo "Use 'make test' to run the tests."

test: test.local

test.local: 
	@echo "**** Testing $(SCANIMAGE) with device $(DEVICE)"
	@if echo "---> Trying flatbed scanner" && \
	  $(SCANIMAGE) -d $(DEVICE) -T && \
	  echo "<--- Flatbed scanner succeded" && \
	  echo "---> Trying three pass flatbed scanner" && \
	  $(SCANIMAGE) -d $(DEVICE) --mode Color --three-pass=yes   -T && \
	  echo "<--- Three pass scanner succeded" && \
	  echo "---> Trying hand scanner" && \
	  $(SCANIMAGE) -d $(DEVICE) --hand-scanner=yes -T && \
	  echo "<--- Hand scanner succeded" && \
	  echo "---> Checking 16 bit color mode" && \
	  $(SCANIMAGE) -d $(DEVICE) $(OPTIONS) && \
	  cmp -s $(TESTFILE) $(OUTFILE) && \
	  echo "<--- 16 bit color mode succeded" && \
	  rm $(OUTFILE) ; \
	then echo ; echo ; echo "**** All tests passed" ; \
	else echo ; echo; \
	echo "**** Something failed (maybe test backend not enabled by configure?)";\
	exit 1; \
	fi
