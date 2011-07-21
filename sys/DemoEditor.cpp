/* DemoEditor.cpp
 *
 * Copyright (C) 2009-2011 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2009/05/04 created
 * pb 2009/05/06 Demo_waitForInput ()
 * pb 2009/05/08 Demo_input ()
 * pb 2009/06/30 removed interpreter member (could cause Praat to crash when the editor was closed after an "execute")
 * pb 2009/08/21 Demo_windowTitle ()
 * pb 2009/12/25 error messages if the user tries to handle the Demo window while it is waiting for input
 * pb 2010/07/13 GTK
 * pb 2011/04/06 C++
 * pb 2011/07/15 C++
 */

#include "DemoEditor.h"
#include "machine.h"
#include "praatP.h"
#include "UnicodeData.h"

static DemoEditor theDemoEditor;

/***** DemoEditor methods *****/

void structDemoEditor :: v_destroy () {
	Melder_free (praatPicture);
	forget (graphics);
	theDemoEditor = NULL;
	DemoEditor_Parent :: v_destroy ();
}

void structDemoEditor :: v_info () {
	DemoEditor_Parent :: v_info ();
	MelderInfo_writeLine2 (L"Colour: ", Graphics_Colour_name (((PraatPicture) praatPicture) -> colour));
	MelderInfo_writeLine2 (L"Font: ", kGraphics_font_getText (((PraatPicture) praatPicture) -> font));
	MelderInfo_writeLine2 (L"Font size: ", Melder_integer (((PraatPicture) praatPicture) -> fontSize));
}

void structDemoEditor :: v_goAway () {
	if (waitingForInput) {
		userWantsToClose = true;
	} else {
		DemoEditor_Parent :: v_goAway ();
	}
}

void structDemoEditor :: v_createMenus () {
	DemoEditor_Parent :: v_createMenus ();
}

static void gui_drawingarea_cb_expose (I, GuiDrawingAreaExposeEvent event) {
	iam (DemoEditor);
	(void) event;
	if (my graphics == NULL) return;   // Could be the case in the very beginning.
	/*
	 * Erase the background. Don't record this erasure!
	 */
	Graphics_stopRecording (my graphics);   // the only place in Praat (the Picture window has a separate Graphics for erasing)?
	Graphics_setColour (my graphics, Graphics_WHITE);
	Graphics_setWindow (my graphics, 0, 1, 0, 1);
	Graphics_fillRectangle (my graphics, 0, 1, 0, 1);
	Graphics_setColour (my graphics, Graphics_BLACK);
	Graphics_startRecording (my graphics);
	Graphics_play (my graphics, my graphics);
}

static void gui_drawingarea_cb_click (I, GuiDrawingAreaClickEvent event) {
	iam (DemoEditor);
	if (my graphics == NULL) return;   // Could be the case in the very beginning.
if (gtk && event -> type != BUTTON_PRESS) return;
	my clicked = true;
	my keyPressed = false;
	my x = event -> x;
	my y = event -> y;
	my key = UNICODE_BULLET;
	my shiftKeyPressed = event -> shiftKeyPressed;
	my commandKeyPressed = event -> commandKeyPressed;
	my optionKeyPressed = event -> optionKeyPressed;
	my extraControlKeyPressed = event -> extraControlKeyPressed;
}

static void gui_drawingarea_cb_key (I, GuiDrawingAreaKeyEvent event) {
	iam (DemoEditor);
	if (my graphics == NULL) return;   // Could be the case in the very beginning.
	my clicked = false;
	my keyPressed = true;
	my x = 0;
	my y = 0;
	my key = event -> key;
	my shiftKeyPressed = event -> shiftKeyPressed;
	my commandKeyPressed = event -> commandKeyPressed;
	my optionKeyPressed = event -> optionKeyPressed;
	my extraControlKeyPressed = event -> extraControlKeyPressed;
}

static void gui_drawingarea_cb_resize (I, GuiDrawingAreaResizeEvent event) {
	iam (DemoEditor);
	if (my graphics == NULL) return;   // Could be the case in the very beginning.
	Dimension marginWidth = 0, marginHeight = 0;
	Graphics_setWsViewport (my graphics, marginWidth, event -> width - marginWidth, marginHeight, event -> height - marginHeight);
	Graphics_setWsWindow (my graphics, 0, 100, 0, 100);
	Graphics_setViewport (my graphics, 0, 100, 0, 100);
	Graphics_updateWs (my graphics);
}

void structDemoEditor :: v_createChildren () {
	drawingArea = GuiDrawingArea_createShown (dialog, 0, 0, 0, 0,
		gui_drawingarea_cb_expose, gui_drawingarea_cb_click, gui_drawingarea_cb_key, gui_drawingarea_cb_resize, this, 0);
	#if gtk
		gtk_widget_set_double_buffered (drawingArea, FALSE);
	#endif
}

class_methods (DemoEditor, Editor) {
	class_methods_end
}

void DemoEditor_init (DemoEditor me, GuiObject parent) {
	Editor_init (me, parent, 0, 0, 1024, 768, L"", NULL);
	my graphics = Graphics_create_xmdrawingarea (my drawingArea);
	Graphics_setColour (my graphics, Graphics_WHITE);
	Graphics_setWindow (my graphics, 0, 1, 0, 1);
	Graphics_fillRectangle (my graphics, 0, 1, 0, 1);
	Graphics_setColour (my graphics, Graphics_BLACK);
	Graphics_startRecording (my graphics);
	//Graphics_setViewport (my graphics, 0, 100, 0, 100);
	//Graphics_setWindow (my graphics, 0, 100, 0, 100);
	//Graphics_line (my graphics, 0, 100, 100, 0);

struct structGuiDrawingAreaResizeEvent event = { my drawingArea, 0 };
event. width = GuiObject_getWidth (my drawingArea);
event. height = GuiObject_getHeight (my drawingArea);
gui_drawingarea_cb_resize (me, & event);
}

DemoEditor DemoEditor_create (GuiObject parent) {
	try {
		autoDemoEditor me = Thing_new (DemoEditor);
		DemoEditor_init (me.peek(), parent);
		return me.transfer();
	} catch (MelderError) {
		Melder_throw ("Demo window not created.");
	}
}

void Demo_open (void) {
	#ifndef CONSOLE_APPLICATION
		if (Melder_batch) {
			/*
			 * Batch scripts have to be able to run demos.
			 */
			//Melder_batch = false;
		}
		if (theDemoEditor == NULL) {
			theDemoEditor = DemoEditor_create ((GuiObject) Melder_topShell);
			Melder_assert (theDemoEditor != NULL);
			//GuiObject_show (theDemoEditor -> dialog);
			theDemoEditor -> praatPicture = Melder_calloc_f (structPraatPicture, 1);
			theCurrentPraatPicture = (PraatPicture) theDemoEditor -> praatPicture;
			theCurrentPraatPicture -> graphics = theDemoEditor -> graphics;
			theCurrentPraatPicture -> font = kGraphics_font_HELVETICA;
			theCurrentPraatPicture -> fontSize = 10;
			theCurrentPraatPicture -> lineType = Graphics_DRAWN;
			theCurrentPraatPicture -> colour = Graphics_BLACK;
			theCurrentPraatPicture -> lineWidth = 1.0;
			theCurrentPraatPicture -> arrowSize = 1.0;
			theCurrentPraatPicture -> x1NDC = 0;
			theCurrentPraatPicture -> x2NDC = 100;
			theCurrentPraatPicture -> y1NDC = 0;
			theCurrentPraatPicture -> y2NDC = 100;
		}
		if (theDemoEditor -> waitingForInput)
			Melder_throw ("You cannot work with the Demo window while it is waiting for input. "
				"Please click or type into the Demo window or close it.");
		theCurrentPraatPicture = (PraatPicture) theDemoEditor -> praatPicture;
	#endif
}

void Demo_close (void) {
	theCurrentPraatPicture = & theForegroundPraatPicture;
}

int Demo_windowTitle (const wchar_t *title) {
	autoDemoOpen demo;
	Thing_setName (theDemoEditor, title);
	return 1;
}

int Demo_show (void) {
	if (theDemoEditor == NULL) return 0;
	autoDemoOpen demo;
	GuiObject_show (theDemoEditor -> dialog);
	GuiWindow_drain (theDemoEditor -> shell);
	return 1;
}

bool Demo_waitForInput (Interpreter interpreter) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	//GuiObject_show (theDemoEditor -> dialog);
	theDemoEditor -> clicked = false;
	theDemoEditor -> keyPressed = false;
	theDemoEditor -> waitingForInput = true;
	#if ! defined (CONSOLE_APPLICATION)
		bool wasBackgrounding = Melder_backgrounding;
		structMelderDir dir = { { 0 } };
		Melder_getDefaultDir (& dir);
		if (wasBackgrounding) praat_foreground ();
		#if gtk
			do {
				gtk_main_iteration ();
			} while (! theDemoEditor -> clicked && ! theDemoEditor -> keyPressed && ! theDemoEditor -> userWantsToClose);
		#else
			do {
				XEvent event;
				GuiNextEvent (& event);
				XtDispatchEvent (& event);
			} while (! theDemoEditor -> clicked && ! theDemoEditor -> keyPressed && ! theDemoEditor -> userWantsToClose);
		#endif
		if (wasBackgrounding) praat_background ();
		Melder_setDefaultDir (& dir);
	#endif
	theDemoEditor -> waitingForInput = false;
	if (theDemoEditor -> userWantsToClose) {
		Interpreter_stop (interpreter);
		Melder_error1 (L"You interrupted the script.");
		forget (theDemoEditor);
		return false;
	}
	return true;
}

bool Demo_clicked (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> clicked;
}

double Demo_x (void) {
	if (theDemoEditor == NULL) return NUMundefined;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return NUMundefined;
	}
	Graphics_setInner (theDemoEditor -> graphics);
	double xWC, yWC;
	Graphics_DCtoWC (theDemoEditor -> graphics, theDemoEditor -> x, theDemoEditor -> y, & xWC, & yWC);
	Graphics_unsetInner (theDemoEditor -> graphics);
	return xWC;
}

double Demo_y (void) {
	if (theDemoEditor == NULL) return NUMundefined;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return NUMundefined;
	}
	Graphics_setInner (theDemoEditor -> graphics);
	double xWC, yWC;
	Graphics_DCtoWC (theDemoEditor -> graphics, theDemoEditor -> x, theDemoEditor -> y, & xWC, & yWC);
	Graphics_unsetInner (theDemoEditor -> graphics);
	return yWC;
}

bool Demo_keyPressed (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> keyPressed;
}

wchar_t Demo_key (void) {
	if (theDemoEditor == NULL) return 0;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return 0;
	}
	return theDemoEditor -> key;
}

bool Demo_shiftKeyPressed (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> shiftKeyPressed;
}

bool Demo_commandKeyPressed (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> commandKeyPressed;
}

bool Demo_optionKeyPressed (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> optionKeyPressed;
}

bool Demo_extraControlKeyPressed (void) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return theDemoEditor -> extraControlKeyPressed;
}

bool Demo_input (const wchar_t *keys) {
	if (theDemoEditor == NULL) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	return wcschr (keys, theDemoEditor -> key) != NULL;
}

bool Demo_clickedIn (double left, double right, double bottom, double top) {
	if (theDemoEditor == NULL || ! theDemoEditor -> clicked) return false;
	if (theDemoEditor -> waitingForInput) {
		Melder_error1 (L"You cannot work with the Demo window while it is waiting for input. "
			"Please click or type into the Demo window or close it.");
		return false;
	}
	double xWC = Demo_x (), yWC = Demo_y ();
	return xWC >= left && xWC < right && yWC >= bottom && yWC < top;
}

/* End of file DemoEditor.cpp */