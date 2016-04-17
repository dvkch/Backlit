--
12/17/97

Most of Jscanimage's main dialog is coded, but only tested with PNM (which
means most of the newly supported option types haven't been tried yet).
Scanning (with PNM) does seem to work, although large files definitely take
longer in JAVA then with "xscanimage".

Main things left to do:
	1.  Provide the "preview" dialog.
	2.  Try it out with a real scanner, especially with large images.
	3.  Try it on a platform other than Linux.

--
11/18/97

This snapshot contains a partially functional "Jscanimage", which is intended
to be a Java clone of "xscanimage".  Only some of the controls are implemented
(sliders, check-boxes, text-entry), and I've only tested it using small images
with the "pnm" backend; but, it does seem to be functional, and to output .pnm
files.

There are also some small changes to the Java API.

To do (at the least):
	1.  Handle "word list" controls.
	2.  Handle "browse" button for file selection.
	3.  Provide pull-down menus for choosing length units, and use them.
	4.  Provide the "preview" dialog.
	5.  Try it out with a real scanner, especially with large images.
	6.  Try it on a platform other than Linux.

	-- Jeff

--
10/17/97

NOTE: To build the Java interface, you have to cd into this directory
and type "make" (after running configure in the top-level directory).

	--david
--
This is my first pass at creating a Java API for SANE.  It requires JDK 1.1.x,
as it uses the new JNI for creating native methods.

This is definitely not 100% pure Java, as it's really just a set of wrappers
which call the actual SANE backend routines.  It's also very preliminary, so
feel free to suggest changes to the interface or class definitions.

The 'install' installs the shared library (libsanej.*), but I'm not sure yet
what to do with the generated Java class files.  So for now, you should be
able to run the test program after installation by typing "java Test" within
the "japi" directory.

-- Jeff Freedman (jsf@hevanet.com)
