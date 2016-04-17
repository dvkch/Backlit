/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Jeffrey S. Freedman
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
   If you do not wish that, delete this exception notice.  */

/**
 **	Jscanimage.java - Java scanner program using SANE.
 **
 **	Written: 10/31/97 - JSF
 **/

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.awt.image.ImageConsumer;
import java.awt.image.ColorModel;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.NumberFormat;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;

/*
 *	Main program.
 */
public class Jscanimage extends Frame implements WindowListener,
			ActionListener, ItemListener, ImageCanvasClient
    {
    //
    //	Static data.
    //
    private static Sane sane;		// The main class.
    private static SaneDevice devList[];// List of devices.
    //
    //	Instance data.
    //
    private Font font;			// For dialog items.
    private int saneHandle;		// Handle of open device.
    private ScanIt scanIt = null;	// Does the actual scan.
					// File to output to.
    private FileOutputStream outFile = null;
    private String outDir = null;	// Stores dir. for output dialog.
    private Vector controls;		// Dialog components for SANE controls.
    private double unitLength = 1;	// # of mm for units to display
					//    (mm = 1, cm = 10, in = 25.4).
    private ImageCanvas imageDisplay = null;
					// "Scan", "Preview" buttons.
    private JButton scanButton, previewButton;
    private JButton browseButton;	// For choosing output filename.
    private JTextField outputField;	// Field for output filename.
    private MenuItem exitMenuItem;	// Menu items.
    private CheckboxMenuItem toolTipsMenuItem;
    private CheckboxMenuItem mmMenuItem;
    private CheckboxMenuItem cmMenuItem;
    private CheckboxMenuItem inMenuItem;

	/*
	 *	Main program.
	 */
    public static void main(String args[])
	{
	if (!initSane())		// Initialize SANE.
		return;
	Jscanimage prog = new Jscanimage();
	prog.pack();
	prog.show();
	}

	/*
	 *	Create main window.
	 */
    public Jscanimage()
	{
	super("SANE Scanimage");
	addWindowListener(this);
					// Open SANE device.
	saneHandle = initSaneDevice(devList[0].name);
	if (saneHandle == 0)
		System.exit(-1);
					// Create scanner class.
	scanIt = new ScanIt(sane, saneHandle);
	init();
	}

	/*
	 *	Clean up.
	 */
    public void finalize()
	{
	if (sane != null)
		{
		if (saneHandle != 0)
			sane.close(saneHandle);
		sane.exit();
		sane = null;
		}
	saneHandle = 0;
	if (outFile != null)
		{
		try
			{
			outFile.close();
			}
		catch (IOException e)
			{ }
		outFile = null;
		}
	System.out.println("In finalize()");
	}

	/*
	 *	Return info.
	 */
    public Sane getSane()
	{ return sane; }
    public int getSaneHandle()
	{ return saneHandle; }
    public double getUnitLength()
	{ return unitLength; }

	/*
	 *	Initialize SANE.
	 */
    private static boolean initSane()
	{
	sane = new Sane();
	int version[] = new int[1];	// Array to get version #.
	int status = sane.init(version);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("getDevices() failed.  Status= " + status);
		return (false);
		}
					// Get list of devices.
					// Allocate room for 50.
	devList = new SaneDevice[50];
	status = sane.getDevices(devList, false);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("getDevices() failed.  Status= " + status);
		return (false);
		}
	for (int i = 0; i < 50 && devList[i] != null; i++)
		{
		System.out.println("Device '" + devList[i].name + "' is a " +
			devList[i].vendor + " " + devList[i].model + " " +
			devList[i].type);
		}
	return (true);
	}

	/*
	 *	Open device.
	 *
	 *	Output:	Handle, or 0 if error.
	 */
    private int initSaneDevice(String name)
	{
	int handle[] = new int[1];
					// Open 1st device, for now.
	int status = sane.open(name, handle);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("open() failed.  Status= " + status);
		return (0);
		}
	setTitle("SANE - " + name);
	System.out.println("Open handle=" + handle[0]);
	return (handle[0]);
	}

	/*
	 *	Add a labeled option to the main dialog.
	 */
    private void addLabeledOption(JPanel group, String title, Component opt,
						GridBagConstraints c)
	{
	JLabel label = new JLabel(title);
	c.gridwidth = GridBagConstraints.RELATIVE;
	c.fill = GridBagConstraints.NONE;
	c.anchor = GridBagConstraints.WEST;
	c.weightx = .1;
	group.add(label, c);
	c.gridwidth = GridBagConstraints.REMAINDER;
	c.fill = GridBagConstraints.HORIZONTAL;
	c.weightx = .4;
	group.add(opt, c);
	}

	/*
	 *	Get options for device.
	 */
    private boolean initSaneOptions()
	{
	JPanel group = null;
	GridBagConstraints c = new GridBagConstraints();
	c.weightx = .4;
	c.weighty = .4;
					// Get # of device options.
	int numDevOptions[] = new int[1];
	int status = sane.getControlOption(saneHandle, 0, numDevOptions, null);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("controlOption() failed.  Status= " 
								+ status);
		return (false);
		}
	System.out.println("Number of device options=" + numDevOptions[0]);
					// Do each option.
	for (int i = 0; i < numDevOptions[0]; i++)
		{
		SaneOption opt = sane.getOptionDescriptor(saneHandle, i);
		if (opt == null)
			{
			System.out.println("getOptionDescriptor() failed for "
						+ i);
			continue;
			}
/*
		System.out.println("Option title: " + opt.title);
		System.out.println("Option desc:  " + opt.desc);
		System.out.println("Option type:  " + opt.type);
 */
		String title;		// Set up title.
		if (opt.unit == SaneOption.UNIT_NONE)
			title = opt.title;
		else			// Show units.
			title = opt.title + " [" + 
					opt.unitString(unitLength) + ']';
		switch (opt.type)
			{
		case SaneOption.TYPE_GROUP:
					// Group for a set of options.
			group = new JPanel(new GridBagLayout());
			c.gridwidth = GridBagConstraints.REMAINDER;
			c.fill = GridBagConstraints.BOTH;
			c.anchor = GridBagConstraints.CENTER;
			add(group, c);
			group.setBorder(new TitledBorder(title));
			break;
		case SaneOption.TYPE_BOOL:
					// A checkbox.
			SaneCheckBox cbox = new SaneCheckBox(opt.title,
						this, i, opt.desc);
			c.gridwidth = GridBagConstraints.REMAINDER;
			c.fill = GridBagConstraints.NONE;
			c.anchor = GridBagConstraints.WEST;
			if (group != null)
				group.add(cbox, c);
			addControl(cbox);
			break;
		case SaneOption.TYPE_FIXED:
					// Fixed-point value.
			if (opt.size != 4)
				break;	// Not sure about this.
			switch (opt.constraintType)
				{
			case SaneOption.CONSTRAINT_RANGE:
					// A scale.
				SaneSlider slider = new FixedSaneSlider(
						opt.rangeConstraint.min, 
						opt.rangeConstraint.max,
						opt.unit == SaneOption.UNIT_MM,
						this, i, opt.desc);
				addLabeledOption(group, title, slider, c);
				addControl(slider);
				break;
			case SaneOption.CONSTRAINT_WORD_LIST:
					// Select from a list.
				SaneFixedBox list = new SaneFixedBox(
							this, i, opt.desc);
				addLabeledOption(group, title, list, c);
				addControl(list);
				break;
				}
			break;
		case SaneOption.TYPE_INT:
					// Integer value.
			if (opt.size != 4)
				break;	// Not sure about this.
			switch (opt.constraintType)
				{
			case SaneOption.CONSTRAINT_RANGE:
					// A scale.
				SaneSlider slider = new SaneSlider(
						opt.rangeConstraint.min, 
						opt.rangeConstraint.max,
						this, i, opt.desc);
				addLabeledOption(group, title, slider, c);
				addControl(slider);
				break;
			case SaneOption.CONSTRAINT_WORD_LIST:
					// Select from a list.
				SaneIntBox list = new SaneIntBox(
							this, i, opt.desc);
				addLabeledOption(group, title, list, c);
				addControl(list);
				break;
				}
			break;
		case SaneOption.TYPE_STRING:
					// Text entry or choice box.
			switch (opt.constraintType)
				{
			case SaneOption.CONSTRAINT_STRING_LIST:
					// Make a list.
				SaneStringBox list = new SaneStringBox(
							this, i, opt.desc);
				addLabeledOption(group, title, list, c);
				addControl(list);
				break;
			case SaneOption.CONSTRAINT_NONE:
				SaneTextField tfield = new SaneTextField(16, 
							this, i, opt.desc);
				addLabeledOption(group, title, tfield, c);
				addControl(tfield);
				break;
				}
			break;
		case SaneOption.TYPE_BUTTON:
			c.gridwidth = GridBagConstraints.REMAINDER;
			c.fill = GridBagConstraints.HORIZONTAL;
			c.anchor = GridBagConstraints.CENTER;
			c.insets = new Insets(8, 4, 4, 4);
			JButton btn = new SaneButton(title, this, i, opt.desc);
			group.add(btn, c);
			c.insets = null;
			break;
		default:
			break;
			}
		}
	return (true);
	}

	/*
	 *	Set up "Output" panel.
	 */
    private JPanel initOutputPanel()
	{
	JPanel panel = new JPanel(new GridBagLayout());
	GridBagConstraints c = new GridBagConstraints();
					// Want 1 row.
	c.gridx = GridBagConstraints.RELATIVE;
	c.gridy = 0;
	c.anchor = GridBagConstraints.WEST;
//	c.fill = GridBagConstraints.HORIZONTAL;
	c.weightx = .4;
	c.weighty = .4;
	panel.setBorder(new TitledBorder("Output"));
	panel.add(new Label("Filename"), c);
	outputField = new JTextField("out.pnm", 16);
	panel.add(outputField, c);
	c.insets = new Insets(0, 8, 0, 0);
	browseButton = new JButton("Browse");
	browseButton.addActionListener(this);
	panel.add(browseButton, c);
	return (panel);
	}

	/*
	 *	Initialize main window.
	 */
    private void init()
	{
	controls = new Vector();	// List of SaneComponent's.
					// Try a light blue:
	setBackground(new Color(140, 200, 255));
					// And a larger font.
	font = new Font("Helvetica", Font.PLAIN, 12);
	setFont(font);
	initMenu();
					// Main panel will be 1 column.
	setLayout(new GridBagLayout());
	GridBagConstraints c = new GridBagConstraints();
	c.gridwidth = GridBagConstraints.REMAINDER;
	c.fill = GridBagConstraints.BOTH;
	c.weightx = .4;
	c.weighty = .4;
					// Panel for output:
	JPanel outputPanel = initOutputPanel();
	add(outputPanel, c);
	initSaneOptions();		// Get SANE options, set up dialog.
	initButtons();			// Put buttons at bottom.
	}

	/*
	 *	Add a control to the dialog and set its initial value.
	 */
    private void addControl(SaneComponent comp)
	{
	controls.addElement(comp);
	comp.setFromControl();		// Get initial value.
	}

	/*
	 *	Reinitialize components from SANE controls.
	 */
    private void reinit()
	{
	Enumeration ctrls = controls.elements();
	while (ctrls.hasMoreElements())	// Do each control.
		{
		SaneComponent comp = (SaneComponent) ctrls.nextElement();
		comp.setFromControl();
		}
	}

	/*
	 *	Set up the menubar.
	 */
    private void initMenu()
	{
	MenuBar mbar = new MenuBar();
	Menu file = new Menu("File");
	mbar.add(file);
	exitMenuItem = new MenuItem("Exit");
	exitMenuItem.addActionListener(this);
	file.add(exitMenuItem);
	Menu pref = new Menu("Preferences");
	mbar.add(pref);
	toolTipsMenuItem = new CheckboxMenuItem("Show tooltips", true);
	toolTipsMenuItem.addItemListener(this);
	pref.add(toolTipsMenuItem);
	Menu units = new Menu("Length unit");
	pref.add(units);
	mmMenuItem = new CheckboxMenuItem("millimeters", true);
	mmMenuItem.addItemListener(this);
	units.add(mmMenuItem);
	cmMenuItem = new CheckboxMenuItem("centimeters", false);
	cmMenuItem.addItemListener(this);
	units.add(cmMenuItem);
	inMenuItem = new CheckboxMenuItem("inches", false);
	inMenuItem.addItemListener(this);
	units.add(inMenuItem);
	setMenuBar(mbar);
	}

	/*
	 *	Set up buttons panel at very bottom.
	 */
    private void initButtons()
	{
					// Buttons are at bottom.
	JPanel bottomPanel = new JPanel(new GridBagLayout());
					// Indent around buttons.
	GridBagConstraints c = new GridBagConstraints();
	c.gridwidth = GridBagConstraints.REMAINDER;
	c.insets = new Insets(8, 8, 8, 8);
	c.weightx = .4;
	c.weighty = .2;
	c.fill = GridBagConstraints.HORIZONTAL;
	add(bottomPanel, c);
	c.weighty = c.weightx = .4;
	c.fill = GridBagConstraints.BOTH;
	c.gridwidth = c.gridheight = 1;
					// Create image display box.
	imageDisplay = new ImageCanvas();
	bottomPanel.add(imageDisplay, c);
					// Put btns. to right in 1 column.
	JPanel buttonsPanel = new JPanel(new GridLayout(0, 1, 8, 8));
	bottomPanel.add(buttonsPanel, c);
	scanButton = new JButton("Scan");
	buttonsPanel.add(scanButton);
	scanButton.addActionListener(this);
	previewButton = new JButton("Preview Window");
	buttonsPanel.add(previewButton);
	previewButton.addActionListener(this);
	}

	/*
	 *	Set a SANE integer option.
	 */
    public void setControlOption(int optNum, int val)
	{
	int [] info = new int[1];
	if (sane.setControlOption(saneHandle, optNum, 
			SaneOption.ACTION_SET_VALUE, val, info) 
							!= Sane.STATUS_GOOD)
		System.out.println("setControlOption() failed.");
	checkOptionInfo(info[0]);
	}

	/*
	 *	Set a SANE text option.
	 */
    public void setControlOption(int optNum, String val)
	{
	int [] info = new int[1];
	if (sane.setControlOption(saneHandle, optNum, 
			SaneOption.ACTION_SET_VALUE, val, info) 
							!= Sane.STATUS_GOOD)
		System.out.println("setControlOption() failed.");
	checkOptionInfo(info[0]);
	}

	/*
	 *	Check the 'info' word returned from setControlOption().
	 */
    private void checkOptionInfo(int info)
	{
					// Does everything completely change?
	if ((info & SaneOption.INFO_RELOAD_OPTIONS) != 0)
		reinit();
					// Need to update status line?
	if ((info & SaneOption.INFO_RELOAD_PARAMS) != 0)
		;			// ++++++++FILL IN.
	}

	/*
	 *	Handle a user action.
	 */
    public void actionPerformed(ActionEvent e)
	{
	if (e.getSource() == scanButton)
		{
		System.out.println("Rescanning");
					// Get file name.
		String fname = outputField.getText();
		if (fname == null || fname.length() == 0)
			fname = "out.pnm";
		try
			{
			outFile = new FileOutputStream(fname);
			}
		catch (IOException oe)
			{
			System.out.println("Error creating file:  " + fname);
			outFile = null;
			return;
			}
					// Disable "Scan" button.
		scanButton.setEnabled(false);
		scanIt.setOutputFile(outFile);
		imageDisplay.setImage(scanIt, this);
		}
	else if (e.getSource() == browseButton)
		{
		File file;		// For working with names.
		FileDialog dlg = new FileDialog(this, "Output Filename",
						FileDialog.SAVE);
		if (outDir != null)	// Set to last directory.
			dlg.setDirectory(outDir);
					// Get current name.
		String fname = outputField.getText();
		if (fname != null)
			{
			file = new File(fname);
			dlg.setFile(file.getName());
			String dname = file.getParent();
			if (dname != null)
				dlg.setDirectory(outDir);
			}
		dlg.show();		// Wait for result.
		fname = dlg.getFile();
					// Store dir. for next time.
		outDir = dlg.getDirectory();
		if (fname != null)
			{
			file = new File(outDir, fname);
			String curDir;
			String fullName;
			try
				{
				curDir = (new File(".")).getCanonicalPath();
				fullName = file.getCanonicalPath();
					// Not in current directory?
				if (!curDir.equals(
					(new File(fullName)).getParent()))
					fname = fullName;
				}
			catch (IOException ex)
				{ curDir = "."; fname = file.getName(); }
			outputField.setText(fname);
			}
		}
	else if (e.getSource() == exitMenuItem)
		{
		finalize();		// Just to be safe.
		System.exit(0);
		}
	}

	/*
	 *	Handle checkable menu items.
	 */
    public void itemStateChanged(ItemEvent e)
	{
	if (e.getSource() == mmMenuItem)
		{			// Wants mm.
		unitLength = 1;
		if (e.getStateChange() == ItemEvent.SELECTED)
			{
			cmMenuItem.setState(false);
			inMenuItem.setState(false);
			reinit();	// Re-display controls.
			}
		else			// Don't let him deselect.
			mmMenuItem.setState(true);
		}
	if (e.getSource() == cmMenuItem)
		{			// Wants cm.
		unitLength = 10;
		if (e.getStateChange() == ItemEvent.SELECTED)
			{
			mmMenuItem.setState(false);
			inMenuItem.setState(false);
			reinit();	// Re-display controls.
			}
		else
			cmMenuItem.setState(true);
		}
	if (e.getSource() == inMenuItem)
		{			// Wants inches.
		unitLength = 25.4;
		if (e.getStateChange() == ItemEvent.SELECTED)
			{
			mmMenuItem.setState(false);
			cmMenuItem.setState(false);
			reinit();	// Re-display controls.
			}
		else			// Don't let him deselect.
			inMenuItem.setState(true);
		}
	}

	/*
	 *	Scan is complete.
	 */
    public void imageDone(boolean error)
	{
	if (error)
		System.out.println("Scanning error.");
	if (outFile != null)		// Close file.
		try
			{
			outFile.close();
			}
		catch (IOException e)
			{ }
	scanButton.setEnabled(true);	// Can scan again.
	}

	/*
	 *	Default window event handlers.
	 */
    public void windowClosed(WindowEvent event)
	{
	finalize();			// Just to be safe.
	}
    public void windowDeiconified(WindowEvent event) { }
    public void windowIconified(WindowEvent event) { }
    public void windowActivated(WindowEvent event) { }
    public void windowDeactivated(WindowEvent event) { }
    public void windowOpened(WindowEvent event) { }
					// Handle closing event.
    public void windowClosing(WindowEvent event)
	{
	finalize();			// Just to be safe.
	System.exit(0);
	}
    }

/*
 *	Interface for our dialog controls.
 */
interface SaneComponent
    {
    public void setFromControl();	// Ask SANE control for current value.
    }

/*
 *	A checkbox in our dialog.
 */
class SaneCheckBox extends JCheckBox implements ActionListener,
					SaneComponent
    {
    private Jscanimage dialog;		// That which created us.
    private int optNum;			// Option #.

	/*
	 *	Create with given state.
	 */
    public SaneCheckBox(String title, Jscanimage dlg, int opt, String tip)
	{
	super(title, false);
	dialog = dlg;
	optNum = opt;
					// Set tool tip.
	if (tip != null && tip.length() != 0)
		super.setToolTipText(tip);
	addActionListener(this);	// Listen to ourself.
	}

	/*
	 *	Update Sane device option with current setting.
	 */
    public void actionPerformed(ActionEvent e)
	{
	System.out.println("In SaneCheckBox::actionPerformed()");
	int val = isSelected() ? 1 : 0;
	dialog.setControlOption(optNum, val);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
	int [] val = new int[1];
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, val, null)
						== Sane.STATUS_GOOD)
		setSelected(val[0] != 0);
	}
    }

/*
 *	A slider in our dialog.  This base class handles integer ranges.
 *	It consists of a slider and a label which shows the current value.
 */
class SaneSlider extends JPanel implements SaneComponent, ChangeListener
    {
    protected Jscanimage dialog;	// That which created us.
    protected int optNum;		// Option #.
    protected JSlider slider;		// The slider itself.
    protected JLabel label;		// Shows current value.

	/*
	 *	Create with given state.
	 */
    public SaneSlider(int min, int max, Jscanimage dlg, int opt, String tip)
	{
	super(new GridBagLayout());	// Create panel.
	dialog = dlg;
	optNum = opt;
	GridBagConstraints c = new GridBagConstraints();
					// Want 1 row.
	c.gridx = GridBagConstraints.RELATIVE;
	c.gridy = 0;
	c.anchor = GridBagConstraints.CENTER;
	c.fill = GridBagConstraints.HORIZONTAL;
	c.weighty = .8;
	c.weightx = .2;			// Give less weight to value label.
	label = new JLabel("00", SwingConstants.RIGHT);
	add(label, c);
	c.weightx = .8;			// Give most weight to slider.
	c.fill = GridBagConstraints.HORIZONTAL;
	slider = new JSlider(JSlider.HORIZONTAL, min, max, 
							min + (max - min)/2);
	add(slider, c);
					// Set tool tip.
	if (tip != null && tip.length() != 0)
		super.setToolTipText(tip);
	slider.addChangeListener(this);	// Listen to ourself.
	}

	/*
	 *	Update Sane device option.
	 */
    public void stateChanged(ChangeEvent e)
	{
	int val = slider.getValue();
	label.setText(String.valueOf(val));
	dialog.setControlOption(optNum, val);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
	int [] val = new int[1];	// Get current SANE control value.
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, val, null)
						== Sane.STATUS_GOOD)
		{
		slider.setValue(val[0]);
		label.setText(String.valueOf(val[0]));
		}
	}
    }

/*
 *	A slider with fixed-point values:
 */
class FixedSaneSlider extends SaneSlider
    {
    private static final int SCALE_MIN = -32000;
    private static final int SCALE_MAX = 32000;
    double min, max;			// Min, max in floating-point.
    boolean optMM;			// Option is in millimeters, so should
					//   be scaled to user's pref.
    private NumberFormat format;	// For printing label.
	/*
	 *	Create with given fixed-point range.
	 */
    public FixedSaneSlider(int fixed_min, int fixed_max, boolean mm,
					Jscanimage dlg, int opt, String tip)
	{
					// Create with large integer range.
	super(SCALE_MIN, SCALE_MAX, dlg, opt, tip);
	format = NumberFormat.getInstance();
					// For now, show 1 decimal point.
	format.setMinimumFractionDigits(1);
	format.setMaximumFractionDigits(1);
					// Store actual range.
	min = dlg.getSane().unfix(fixed_min);
	max = dlg.getSane().unfix(fixed_max);
	optMM = mm;
	}

	/*
	 *	Update Sane device option.
	 */
    public void stateChanged(ChangeEvent e)
	{
	double val = (double) slider.getValue();
					// Convert to actual control scale.
	val = min + ((val - SCALE_MIN)/(SCALE_MAX - SCALE_MIN)) * (max - min);
	label.setText(format.format(optMM ? 
					val/dialog.getUnitLength() : val));
	dialog.setControlOption(optNum, dialog.getSane().fix(val));
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
	int [] ival = new int[1];	// Get current SANE control value.
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, ival, null)
						== Sane.STATUS_GOOD)
		{
		double val = dialog.getSane().unfix(ival[0]);
					// Show value with user's pref.
		label.setText(format.format(optMM ? 
					val/dialog.getUnitLength() : val));
		val = SCALE_MIN + ((val - min)/(max - min)) * 
						(SCALE_MAX - SCALE_MIN);
		slider.setValue((int) val);
		}
	}
    }

/*
 *	A Button in our dialog.
 */
class SaneButton extends JButton implements ActionListener
    {
    private Jscanimage dialog;		// That which created us.
    private int optNum;			// Option #.

	/*
	 *	Create with given state.
	 */
    public SaneButton(String title, Jscanimage dlg, int opt, String tip)
	{
	super(title);
	dialog = dlg;
	optNum = opt;
					// Set tool tip.
	if (tip != null && tip.length() != 0)
		super.setToolTipText(tip);
	addActionListener(this);	// Listen to ourself.
	}

	/*
	 *	Update Sane device option.
	 */
    public void actionPerformed(ActionEvent e)
	{
	dialog.setControlOption(optNum, 0);
	System.out.println("In SaneButton::actionPerformed()");
	}
    }

/*
 *	A combo-box for showing a list of items.
 */
abstract class SaneComboBox extends JComboBox 
				implements SaneComponent, ItemListener
    {
    protected Jscanimage dialog;	// That which created us.
    protected int optNum;		// Option #.

	/*
	 *	Create it.
	 */
    public SaneComboBox(Jscanimage dlg, int opt, String tip)
	{
	dialog = dlg;
	optNum = opt;
					// Set tool tip.
	if (tip != null && tip.length() != 0)
		super.setToolTipText(tip);
	addItemListener(this);
	}

	/*
	 *	Select desired item by its text.
	 */
    protected void select(String text)
	{
	int cnt = getItemCount();
	for (int i = 0; i < cnt; i++)
		if (text.equals(getItemAt(i)))
			{
			setSelectedIndex(i);
			break;
			}
	}

    abstract public void setFromControl();
    abstract public void itemStateChanged(ItemEvent e);
    }

/*
 *	A list of strings.
 */
class SaneStringBox extends SaneComboBox
    {

	/*
	 *	Create it.
	 */
    public SaneStringBox(Jscanimage dlg, int opt, String tip)
	{
	super(dlg, opt, tip);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
					// Let's get the whole list.
	SaneOption opt = dialog.getSane().getOptionDescriptor(
					dialog.getSaneHandle(), optNum);
	if (opt == null)
		return;
	removeAllItems();		// Clear list.
					// Go through list from option.
	for (int i = 0; opt.stringListConstraint[i] != null; i++)
		addItem(opt.stringListConstraint[i]);
					// Get value.
	byte buf[] = new byte[opt.size + 1];
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, buf, null)
						== Sane.STATUS_GOOD)
		select(new String(buf));
	}

	/*
	 *	An item was selected.
	 */
    public void itemStateChanged(ItemEvent e)
	{
				// Get current selection.
	String val = (String) getSelectedItem();
	if (val != null)
		dialog.setControlOption(optNum, val);
	}
    }

/*
 *	A list of integers.
 */
class SaneIntBox extends SaneComboBox
    {

	/*
	 *	Create it.
	 */
    public SaneIntBox(Jscanimage dlg, int opt, String tip)
	{
	super(dlg, opt, tip);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
					// Let's get the whole list.
	SaneOption opt = dialog.getSane().getOptionDescriptor(
					dialog.getSaneHandle(), optNum);
	if (opt == null)
		return;
	removeAllItems();		// Clear list.
					// Go through list from option.
	int cnt = opt.wordListConstraint[0];
	for (int i = 0; i < cnt; i++)
		addItem(String.valueOf(opt.wordListConstraint[i + 1]));
					// Get value.
	int [] val = new int[1];	// Get current SANE control value.
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, val, null)
						== Sane.STATUS_GOOD)
		select(String.valueOf(val[0]));
	}

	/*
	 *	An item was selected.
	 */
    public void itemStateChanged(ItemEvent e)
	{
				// Get current selection.
	String val = (String) getSelectedItem();
	if (val != null)
		try
			{
			Integer v = Integer.valueOf(val);
			dialog.setControlOption(optNum, v.intValue());
			}
		catch (NumberFormatException ex) {  }
	}
    }

/*
 *	A list of fixed-point floating point numbers.
 */
class SaneFixedBox extends SaneComboBox
    {

	/*
	 *	Create it.
	 */
    public SaneFixedBox(Jscanimage dlg, int opt, String tip)
	{
	super(dlg, opt, tip);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
					// Let's get the whole list.
	SaneOption opt = dialog.getSane().getOptionDescriptor(
					dialog.getSaneHandle(), optNum);
	if (opt == null)
		return;
	removeAllItems();		// Clear list.
					// Go through list from option.
	int cnt = opt.wordListConstraint[0];
	for (int i = 0; i < cnt; i++)
		addItem(String.valueOf(dialog.getSane().unfix(
					opt.wordListConstraint[i + 1])));
					// Get value.
	int [] val = new int[1];	// Get current SANE control value.
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, val, null)
						== Sane.STATUS_GOOD)
		select(String.valueOf(dialog.getSane().unfix(val[0])));
	}

	/*
	 *	An item was selected.
	 */
    public void itemStateChanged(ItemEvent e)
	{
				// Get current selection.
	String val = (String) getSelectedItem();
	if (val != null)
		try
			{
			Double v = Double.valueOf(val);
			dialog.setControlOption(optNum,
					dialog.getSane().fix(v.doubleValue()));
			}
		catch (NumberFormatException ex) {  }
	}
    }

/*
 *	A text-entry field in our dialog.
 */
class SaneTextField extends JTextField implements SaneComponent
    {
    private Jscanimage dialog;		// That which created us.
    private int optNum;			// Option #.

	/*
	 *	Create with given state.
	 */
    public SaneTextField(int width, Jscanimage dlg, int opt, String tip)
	{
	super(width);			// Set initial text, # chars.
	dialog = dlg;
	optNum = opt;
					// Set tool tip.
	if (tip != null && tip.length() != 0)
		super.setToolTipText(tip);
	}

	/*
	 *	Update Sane device option with current setting when user types.
	 */
    public void processKeyEvent(KeyEvent e)
	{
	super.processKeyEvent(e);	// Handle it normally.
	if (e.getID() != KeyEvent.KEY_TYPED)
		return;			// Just do it after keystroke.
	String userText = getText();	// Get what's there, & save copy.
	String newText = new String(userText);
	dialog.setControlOption(optNum, newText);
	if (!newText.equals(userText))	// Backend may have changed it.
		setText(newText);
	}

	/*
	 *	Update from Sane control's value.
	 */
    public void setFromControl()
	{
	byte buf[] = new byte[1024];
	if (dialog.getSane().getControlOption(
			dialog.getSaneHandle(), optNum, buf, null)
						== Sane.STATUS_GOOD)
		{
		String text = new String(buf);
		System.out.println("SaneTextField::setFromControl: " + text);
		setText(text);		// Set text in field.
		setScrollOffset(0);
		}
	}
    }
