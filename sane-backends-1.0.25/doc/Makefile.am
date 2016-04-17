##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

man1_MANS = scanimage.1 sane-config.1 sane-find-scanner.1 gamma4scanimage.1
EXTRA_DIST = scanimage.man sane-config.man sane-find-scanner.man \
  gamma4scanimage.man

# Create a list of man pages for only backends that are compiled in so
# that is all that gets installed.  Also, build a complete list of all
# backend man pages.  This allows a user to optionally generate all
# man pages in HTML form for things like posting to a web site.
#
# Backend man pages bypass standard automake man5_MANS because there is not
# a man page for every supported backend.  We could get rid of
# custom install/uninstall if we required man pages for every backend.
BACKEND_MANS_ENABLED = @BACKEND_MANS_ENABLED@
BACKEND_5MANS = sane-abaton.5 sane-agfafocus.5 sane-apple.5 sane-as6e.5 \
	sane-dll.5 sane-dc25.5 sane-dmc.5 sane-epson.5 sane-epson2.5 sane-epsonds.5 \
	sane-hp.5 sane-gphoto2.5 sane-leo.5 sane-lexmark.5 \
	sane-matsushita.5 sane-microtek.5 sane-microtek2.5 sane-mustek.5 \
	sane-nec.5 sane-net.5 sane-pie.5 sane-pieusb.5 sane-pint.5 sane-pnm.5 \
	sane-umax.5 sane-qcam.5 sane-scsi.5 sane-artec.5 sane-kodak.5 sane-kodakaio.5 \
	sane-fujitsu.5 sane-sharp.5 sane-s9036.5 sane-tamarack.5 \
	sane-ricoh.5 sane-avision.5 sane-plustek.5 sane-st400.5 \
	sane-mustek_pp.5 sane-dc210.5 sane-v4l.5 sane-snapscan.5 \
	sane-canon.5 sane-coolscan.5 sane-bh.5 sane-dc240.5 \
	sane-umax_pp.5 sane-umax1220u.5 sane-sm3600.5 sane-usb.5 \
     	sane-mustek_usb.5 sane-sceptre.5 sane-canon_pp.5 sane-canon630u.5 \
        sane-teco1.5 sane-teco2.5 sane-teco3.5 sane-test.5 sane-sp15c.5 \
	sane-coolscan2.5 sane-hpsj5s.5 sane-gt68xx.5 sane-artec_eplus48u.5 \
	sane-ma1509.5 sane-ibm.5 sane-hp5400.5 sane-plustek_pp.5 sane-u12.5 \
	sane-niash.5 sane-sm3840.5 sane-genesys.5 sane-hp4200.5 \
	sane-mustek_usb2.5 sane-hp3500.5 sane-pixma.5 sane-stv680.5 \
	sane-hp5590.5 sane-hpljm1005.5 sane-cardscan.5 sane-hp3900.5 \
	sane-epjitsu.5 sane-hs2p.5 sane-canon_dr.5 sane-xerox_mfp.5 \
	sane-rts8891.5 sane-coolscan3.5 sane-kvs1025.5 sane-kvs20xx.5 \
	sane-kvs40xx.5 sane-p5.5 sane-magicolor.5

EXTRA_DIST += sane-abaton.man sane-agfafocus.man sane-apple.man sane-as6e.man \
	sane-dll.man sane-dc25.man sane-dmc.man sane-epson.man \
	sane-epson2.man sane-epsonds.man sane-hp.man sane-gphoto2.man sane-leo.man \
	sane-lexmark.man sane-matsushita.man sane-microtek.man \
	sane-microtek2.man sane-mustek.man sane-nec.man sane-net.man \
	sane-pie.man sane-pieusb.man sane-pint.man sane-pnm.man sane-umax.man \
	sane-qcam.man sane-scsi.man sane-artec.man sane-fujitsu.man \
	sane-sharp.man sane-s9036.man sane-tamarack.man sane-kodak.man sane-kodakaio.man\
	sane-ricoh.man sane-avision.man sane-plustek.man sane-st400.man \
	sane-mustek_pp.man sane-dc210.man sane-v4l.man sane-snapscan.man \
	sane-canon.man sane-coolscan.man sane-bh.man sane-dc240.man \
	sane-umax_pp.man sane-umax1220u.man sane-sm3600.man sane-usb.man \
     	sane-mustek_usb.man sane-sceptre.man sane-canon_pp.man \
	sane-canon630u.man sane-teco1.man sane-teco2.man \
	sane-teco3.man sane-test.man sane-sp15c.man sane-coolscan2.man \
	sane-hpsj5s.man sane-gt68xx.man sane-artec_eplus48u.man \
	sane-ma1509.man sane-ibm.man sane-hp5400.man sane-plustek_pp.man \
	sane-u12.man sane-niash.man sane-sm3840.man sane-genesys.man \
	sane-hp4200.man sane-mustek_usb2.man sane-hp3500.man \
	sane-pixma.man sane-stv680.man sane-hp5590.man sane-hpljm1005.man \
	sane-cardscan.man sane-hp3900.man sane-epjitsu.man sane-hs2p.man \
	sane-canon_dr.man sane-xerox_mfp.man sane-rts8891.man \
	sane-coolscan3.man sane-kvs1025.man sane-kvs20xx.man sane-kvs40xx.man \
	sane-p5.man sane-magicolor.man

man7_MANS = sane.7
EXTRA_DIST += sane.man

man8_MANS = saned.8
EXTRA_DIST += saned.man

MANPAGES = $(man1_MANS) $(BACKEND_5MANS) $(man7_MANS) $(man8_MANS)

if CROSS_COMPILING
HTML_PAGES =
else
HTML_PAGES = sane-backends.html sane-backends-external.html \
	     sane-mfgs.html sane-mfgs-external.html
endif
doc_DATA = $(HTML_PAGES)

if USE_LATEX
STANDARD = sane.ps
else
STANDARD =
endif

all: bemans $(STANDARD) html-pages

dist_doc_DATA = backend-writing.txt
nobase_dist_doc_DATA = $(BEDOCS)
doc_DATA += $(STANDARD)
EXTRA_DIST += descriptions.txt releases.txt sane-logo2.jpg sane-logo.png \
  sane.png

LATEX	= TEXINPUTS="$(srcdir):$$TEXINPUTS" @LATEX@
DVIPS   = @DVIPS@
MAKEINDEX = @MAKEINDEX@
DLH	= TEXINPUTS="$(srcdir):$$TEXINPUTS" dlh
MAN2HTML= nroff -c -man |\
	  man2html -compress -topm 6 -botm 6 -nodepage -title $${page} \
          -cgiurl '$$title.$$section.html'|\
	  sed 's,<BODY>,<BODY BGCOLOR=\#FFFFFF TEXT=\#000000><H1 ALIGN=CENTER><IMG SRC="/images/sane.png" HEIGHT=117 WIDTH=346></H1>,'

%.1 %.5 %.7 %.8: %.man
	@echo "Generating manpage $@...";
	@sed -e 's|@DATADIR@|$(datadir)|g' \
	  -e 's|@CONFIGDIR@|$(configdir)|g' \
	  -e 's|@DOCDIR@|$(docdir)|g' \
	  -e 's|@LIBDIR@|$(libdir)/sane|g' \
	  -e 's|@BINDIR@|$(bindir)|g' \
	  -e 's|@SBINDIR@|$(sbindir)|g' \
	  -e 's|@PACKAGEVERSION@|$(package_version)|g' $^ >$@

bemans: $(BACKEND_5MANS)

beman5dir = $(mandir)/man5
install-beman5: $(BACKEND_5MANS)
	test -z "$(beman5dir)" || $(MKDIR_P) "$(DESTDIR)$(beman5dir)"
	@list="$(BACKEND_MANS_ENABLED) sane-dll.5 sane-scsi.5 sane-usb.5"; for man in $$list; do \
	  if test ! -r $${man}; then continue; fi; \
	  echo "installing $${man} in $(beman5dir)/$${man}..."; \
	  $(INSTALL_DATA) $${man} $(DESTDIR)$(beman5dir)/$${man} \
	  || exit 1; \
	done

BEDOCS = canon/canon.changes canon/canon.install2700F.txt
BEDOCS += gt68xx/gt68xx.CHANGES gt68xx/gt68xx.TODO
BEDOCS += leo/leo.txt
BEDOCS += matsushita/matsushita.txt
BEDOCS += mustek/mustek.CHANGES
BEDOCS += mustek_usb/mustek_usb.CHANGES mustek_usb/mustek_usb.TODO
BEDOCS += mustek_usb2/mustek_usb2.CHANGES mustek_usb2/mustek_usb2.TODO
BEDOCS += niash/niash.TODO
BEDOCS += plustek/FAQ plustek/MakeModule.sh plustek/Makefile.kernel24 \
  plustek/Makefile.kernel26 plustek/Plustek-PARPORT-TODO.txt \
  plustek/Plustek-PARPORT.changes plustek/Plustek-PARPORT.txt \
  plustek/Plustek-USB-TODO.txt plustek/Plustek-USB.changes \
  plustek/Plustek-USB.txt
BEDOCS += sceptre/s1200.txt
BEDOCS += teco/teco1.txt teco/teco2.txt teco/teco3.txt
BEDOCS += u12/U12.changes u12/U12.todo
BEDOCS += umax/negative-types.txt umax/sane-logo.jpg \
  umax/sane-umax-advanced-options-doc.html \
  umax/sane-umax-advanced.jpg umax/sane-umax-astra-doc.html \
  umax/sane-umax-config-doc.html umax/sane-umax-doc.html \
  umax/sane-umax-histogram.jpg umax/sane-umax-mirage-doc.html \
  umax/sane-umax-not-listed-doc.html umax/sane-umax-others-doc.html \
  umax/sane-umax-parport-doc.html umax/sane-umax-powerlook-doc.html \
  umax/sane-umax-scanner-clones-doc.html umax/sane-umax-speed-doc.html \
  umax/sane-umax-standard-options-doc.html \
  umax/sane-umax-standard.jpg umax/sane-umax-text.jpg \
  umax/sane-umax-text2.jpg umax/sane-umax-text4.jpg \
  umax/sane-umax-uc-doc.html umax/sane-umax-vista-doc.html \
  umax/sane-umax.jpg umax/umax.BUGS umax/umax.CHANGES umax/umax.FAQ \
  umax/umax.TODO

EXTRA_DIST += sane.tex net.tex html.sty

DESC_FILES = descriptions/abaton.desc descriptions/agfafocus.desc \
  descriptions/apple.desc descriptions/artec.desc \
  descriptions/artec_eplus48u.desc descriptions/as6e.desc \
  descriptions/avision.desc descriptions/bh.desc descriptions/canon630u.desc \
  descriptions/canon.desc descriptions/canon_dr.desc \
  descriptions/canon_pp.desc descriptions/cardscan.desc \
  descriptions/coolscan2.desc descriptions/coolscan.desc \
  descriptions/coolscan3.desc \
  descriptions/dc210.desc descriptions/dc240.desc descriptions/dc25.desc \
  descriptions/dell1600n_net.desc descriptions/dll.desc descriptions/dmc.desc \
  descriptions/epjitsu.desc descriptions/epson2.desc descriptions/epson.desc \
  descriptions/epsonds.desc \
  descriptions/fujitsu.desc descriptions/genesys.desc \
  descriptions/gphoto2.desc descriptions/gt68xx.desc descriptions/hp3500.desc \
  descriptions/hp3900.desc descriptions/hp4200.desc descriptions/hp5400.desc \
  descriptions/hp5590.desc descriptions/hp.desc descriptions/hpljm1005.desc \
  descriptions/hpsj5s.desc descriptions/hs2p.desc descriptions/ibm.desc \
  descriptions/kodak.desc descriptions/kodakaio.desc descriptions/kvs1025.desc \
  descriptions/kvs20xx.desc descriptions/kvs40xx.desc \
  descriptions/leo.desc descriptions/lexmark.desc descriptions/ma1509.desc \
  descriptions/magicolor.desc \
  descriptions/matsushita.desc descriptions/microtek2.desc \
  descriptions/microtek.desc descriptions/mustek.desc \
  descriptions/mustek_pp.desc descriptions/mustek_usb2.desc \
  descriptions/mustek_usb.desc descriptions/nec.desc descriptions/net.desc \
  descriptions/niash.desc descriptions/pie.desc descriptions/pieusb.desc descriptions/pint.desc \
  descriptions/p5.desc descriptions/pixma.desc descriptions/plustek.desc \
  descriptions/plustek_pp.desc descriptions/pnm.desc descriptions/qcam.desc \
  descriptions/ricoh.desc descriptions/rts8891.desc descriptions/s9036.desc \
  descriptions/sceptre.desc descriptions/sharp.desc descriptions/sm3600.desc \
  descriptions/sm3840.desc descriptions/snapscan.desc descriptions/sp15c.desc \
  descriptions/st400.desc descriptions/stv680.desc descriptions/tamarack.desc \
  descriptions/teco1.desc descriptions/teco2.desc descriptions/teco3.desc \
  descriptions/test.desc descriptions/u12.desc \
  descriptions/umax1220u.desc descriptions/umax.desc \
  descriptions/umax_pp.desc  descriptions/unsupported.desc \
  descriptions/v4l.desc descriptions/xerox_mfp.desc
EXTRA_DIST += $(DESC_FILES) descriptions/template.desc. 

DESC_EXT_FILES = descriptions-external/brother2.desc \
  descriptions-external/brother.desc \
  descriptions-external/brother-mfc4600.desc \
  descriptions-external/canon_mfp.desc \
  descriptions-external/cs3200f.desc \
  descriptions-external/epkowa.desc \
  descriptions-external/geniusvp2.desc \
  descriptions-external/hp3770.desc \
  descriptions-external/hp8200.desc \
  descriptions-external/hpaio.desc \
  descriptions-external/hpoj.desc \
  descriptions-external/kodak-twain.desc \
  descriptions-external/lhii.desc \
  descriptions-external/mustek_a3p1.desc \
  descriptions-external/panamfs.desc \
  descriptions-external/primascan.desc \
  descriptions-external/primax.desc \
  descriptions-external/samsung.desc \
  descriptions-external/scanwit.desc \
  descriptions-external/v4l2.desc \
  descriptions-external/viceo.desc
EXTRA_DIST += $(DESC_EXT_FILES) descriptions-external/template.desc.

EXTRA_DIST += figs/area.eps figs/area.fig figs/flow.eps figs/flow.fig \
  figs/hierarchy.eps figs/hierarchy.fig figs/image-data.eps \
  figs/image-data.fig figs/xfer.eps figs/xfer.fig

EXTRA_DIST += icons/contents.gif icons/index.gif icons/next.gif \
  icons/next_gr.gif icons/previous.gif icons/previous_gr.gif \
  icons/references.gif icons/references_gr.gif icons/up.gif icons/up_gr.gif

install-data-local: install-beman5

uninstall-local:
	rm -rf $(DESTDIR)$(beman5dir)/sane-*.5

sane.ind: $(srcdir)/sane.tex
	@echo Generating index for sane.tex...
	@touch sane.ind
	@$(LATEX) $(srcdir)/sane </dev/null >/dev/null && \
	  $(MAKEINDEX) sane.idx 2>/dev/null && \
	  $(LATEX) $(srcdir)/sane </dev/null >/dev/null

sane.dvi: sane.ind
	@echo Generating sane.dvi from sane.tex...
	@$(LATEX) $(srcdir)/sane </dev/null >/dev/null

sane.ps: sane.dvi
	@echo Generating sane.ps from sane.dvi...
	@$(DVIPS) sane.dvi -o sane.ps 2>/dev/null

sane-html: sane.dvi
	$(DLH) $(srcdir)/sane.tex

html-man: $(MANPAGES)
	@for page in $(MANPAGES); do \
	  echo "translating $${page} to $${page}.html..."; \
	  cat $${page} | $(MAN2HTML) > $${page}.html; \
	done

../tools/sane-desc:
	cd ../tools ; $(MAKE) sane-desc

sane-backends.html: $(DESC_FILES) ../tools/sane-desc
	@../tools/sane-desc -m html-backends-split -s ${srcdir}/descriptions \
	  -i "The following table summarizes the backends/drivers distributed \
	  with $(PACKAGE)-$(VERSION), and the hardware or software they \
	  support." -d 0 > sane-backends.html

sane-backends-external.html: $(DESC_EXT_FILES) ../tools/sane-desc
	@../tools/sane-desc -m html-backends-split \
	  -s ${srcdir}/descriptions-external \
	  -i "The following table summarizes the backends/drivers that \
	  have not yet been included in the SANE distribution, and the \
	  hardware or software they support." \
	  -t "SANE: External Backends (Drivers)" -d 0 \
	  > sane-backends-external.html

sane-mfgs.html: $(DESC_FILES) ../tools/sane-desc
	@../tools/sane-desc -m html-mfgs -s ${srcdir}/descriptions \
	  -i "The following table summarizes the devices supported \
	  by $(PACKAGE)-$(VERSION)." \
	  -d 0 > sane-mfgs.html

sane-mfgs-external.html: $(DESC_EXT_FILES) ../tools/sane-desc
	@../tools/sane-desc -m html-mfgs -s ${srcdir}/descriptions-external \
	  -i "The following table summarizes the devices that \
	  are supported by external backends." \
	  -t "SANE: Devices Supported by External Backends" -d 0 \
	  > sane-mfgs-external.html

descriptions.db: $(DESC_FILES) ../tools/sane-desc
	@../tools/sane-desc -m ascii -s ${srcdir}/descriptions \
	  > descriptions.db

descriptions-external.db: $(DESC_FILES) ../tools/sane-desc
	@../tools/sane-desc -m ascii -s ${srcdir}/descriptions-external \
	  > descriptions-external.db

html-pages: $(HTML_PAGES)
html: html-pages html-man sane-html

clean-local:
	rm -f *.toc *.aux *.log *.cp *.fn *.tp *.vr *.pg *.ky *.blg *.idx *.cb
	rm -f *.ilg

distclean-local:
	rm -f $(MANPAGES)
	rm -f *.lot *.lof *.ind
	rm -f sane.dvi sane.ps sane-backends.html sane-backends-external.html
	rm -f sane-mfgs.html sane-mfgs-external.html
	rm -f sane/*.html sane/*.gif
	rm -f doxygen-sanei.conf doxygen-genesys.conf
	-rm -rf sane sanei-html
	for manpage in $(MANPAGES) ; do \
	  rm -f $${manpage}.html ; \
	done
