//
// File "gwindow.h"
// Simple graphic window, based on Xlib primitives
//

#ifndef _GWINDOW_H
#define _GWINDOW_H

#include<stdint.h>

// Classes for simple 2-dimensional objects
#include <R2Graph.h>

// include the X library headers
//extern "C" {
/*
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
*/
#include<xcb/xcb.h>
#include<xcb/xcb_keysyms.h>
//}

//===============================

class ListHeader {
public:
    ListHeader*     next;
    ListHeader*     prev;

public:
    ListHeader():
        next(0),
        prev(0)
    {
    }

    ListHeader(ListHeader* n, ListHeader* p):
        next(n),
        prev(p)
    {
    }

    ListHeader(const ListHeader& h):
        next(h.next),
        prev(h.prev)
    {
    }

    ~ListHeader() {}

    ListHeader& operator=(const ListHeader& h) {
        next = h.next;
        prev = h.prev;
        return *this;
    }

    void link(ListHeader& h) {
        next = &h;
        h.prev = this;
    }
};

class FontDescriptor: public ListHeader {
public:
    // Font font_id;
    // XFontStruct* font_struct;
    xcb_font_t font_id;
    xcb_query_font_reply_t *font_info;

    FontDescriptor(xcb_font_t id, xcb_query_font_reply_t *info):
        ListHeader(),
        font_id(id),
        font_info(info)
    {}
};


const int DEFAULT_BORDER_WIDTH = 2;

class GWindow: public ListHeader {
public:
    // XCB objects:
    // Connection and screen are the same for all windows

	static xcb_connection_t*	m_Connection;
	static int					m_Screen;
	static xcb_screen_t*		m_ScreenInfo;
	static xcb_atom_t			m_WMProtocolsAtom;
	static xcb_atom_t			m_WMDeleteWindowAtom;
    static xcb_key_symbols_t*   m_KeySymbols;

	xcb_window_t m_Window;
	xcb_pixmap_t m_Pixmap;
	xcb_gcontext_t m_GC;


    // Coordinates in window
    I2Point     m_WindowPosition;   // Window position in screen coord
    I2Rectangle m_IWinRect; // Window rectangle in (local) pixel coordinates
    R2Rectangle m_RWinRect; // Window rectangle in inner (real) coordinates

    I2Point m_ICurPos; // Current position, integer
    R2Point m_RCurPos; //                   real

    double m_xcoeff;  // Optimization: Coeff. for real->integer conversion
    double m_ycoeff;

    char   m_WindowTitle[128];

    // Window state
    bool        m_WindowCreated;

    // Static members:
    // List of all windows
    static int          m_NumWindows;
    static int          m_NumCreatedWindows;
    static ListHeader   m_WindowList;
    static ListHeader   m_FontList;

protected:

    // Background, foreground
    unsigned long       m_bgPixel;
    unsigned long       m_fgPixel;
    const char*         m_bgColorName;
    const char*         m_fgColorName;

    // Border width
    int m_BorderWidth;

    // Clip rectangle
    // XRectangle       m_ClipRectangle;
	xcb_rectangle_t		m_ClipRectangle;
    bool                m_BeginExposeSeries;

public:

    GWindow();
    GWindow(const I2Rectangle& frameRect, const char *title = 0);
    GWindow(
        const I2Rectangle& frameRect, 
        const R2Rectangle& coordRect,
        const char *title = 0
    );
    virtual ~GWindow();

    // The "createWindow" method uses the m_IWinRect member to
    // define the position and size of the window, and the
    // m_WindowTitle member to set the window title.
    // All parameters have their default values, so the method can
    // be called without any parameters: createWindow()
    void createWindow(
        GWindow* parentWindow = 0,							            // parent window
        int borderWidth = DEFAULT_BORDER_WIDTH,				            // border width
        xcb_window_class_t wndClass = XCB_WINDOW_CLASS_INPUT_OUTPUT,	// or INPUT_ONLY, COPY_FROM_PARENT
        xcb_visualid_t visual = XCB_COPY_FROM_PARENT,		            //
        uint32_t attributesValueMask = 0,				            // which attributes are defined
        void* attributes = NULL								            // attributes structure
    );
    void createWindow(
        const I2Rectangle& frameRect, 
        const char *title = 0,
        GWindow* parentWindow = 0,
        int borderWidth = DEFAULT_BORDER_WIDTH
    );
    void createWindow(
        const I2Rectangle& frameRect, 
        const R2Rectangle& coordRect,
        const char *title = 0,
        GWindow* parentWindow = 0,
        int borderWidth = DEFAULT_BORDER_WIDTH
    );

    static bool initX();
    static void closeX();
    static int  screenMaxX();
    static int  screenMaxY();
    static void releaseFonts();

private:
    static GWindow* findWindow(xcb_window_t w);

    static FontDescriptor* findFont(xcb_font_t fontID);
    static void removeFontDescriptor(FontDescriptor* fd);
    static void addFontDescriptor(
        xcb_font_t fontID, xcb_query_font_reply_t* fontInfo
    );

public:
    void drawFrame();
    void setCoordinates(double xmin, double ymin, double xmax, double ymax);
    void setCoordinates(const R2Rectangle& coordRect);
    void setCoordinates(const R2Point& leftBottom, const R2Point& rightTop);

    double getXMin() const { return m_RWinRect.getXMin(); }
    double getXMax() const { return m_RWinRect.getXMax(); }
    double getYMin() const { return m_RWinRect.getYMin(); }
    double getYMax() const { return m_RWinRect.getYMax(); }

    void drawAxes(
        const char *axesColorName = 0,
        bool drawGrid = false,
        const char *gridColorName = 0,
        bool offscreen = false
    );

    I2Rectangle getWindowRect() const { return m_IWinRect; }
    R2Rectangle getCoordRect() const  { return m_RWinRect; }

    void moveTo(const I2Point& p);
    void moveTo(const R2Point& p);
    void moveTo(int x, int y);
    void moveTo(double x, double y);

    void moveRel(const I2Vector& p);
    void moveRel(const R2Vector& p);
    void moveRel(int x, int y);
    void moveRel(double x, double y);

    void drawLineTo(const I2Point& p, bool offscreen = false);
    void drawLineTo(const R2Point& p, bool offscreen = false);
    void drawLineTo(int x, int y, bool offscreen = false);
    void drawLineTo(double x, double y, bool offscreen = false);

    void drawLineRel(const I2Vector& p, bool offscreen = false);
    void drawLineRel(const R2Vector& p, bool offscreen = false);
    void drawLineRel(int x, int y, bool offscreen = false);
    void drawLineRel(double x, double y, bool offscreen = false);

    void drawLine(const I2Point& p1, const I2Point& p2, bool offscreen = false);
    void drawLine(const I2Point& p,  const I2Vector& v, bool offscreen = false);
    void drawLine(int x1, int y1, int x2, int y2, bool offscreen = false);

    void drawLine(const R2Point& p1, const R2Point& p2, bool offscreen = false);
    void drawLine(const R2Point& p,  const R2Vector& v, bool offscreen = false);
    void drawLine(double x1, double y1, double x2, double y2, bool offscreen = false);

    void drawEllipse(const I2Rectangle&, bool offscreen = false);
    void drawEllipse(const R2Rectangle&, bool offscreen = false);
    void drawCircle(const I2Point& center, int radius, bool offscreen = false);
    void drawCircle(const R2Point& center, double radius, bool offscreen = false);

    void drawString(int x, int y, const char *str, int len = (-1), bool offscreen = false);
    void drawString(const I2Point& p, const char *str, int len = (-1), bool offscreen = false);
    void drawString(const R2Point& p, const char *str, int len = (-1), bool offscreen = false);
    void drawString(double x, double y, const char *str, int len = (-1), bool offscreen = false);

    // The following methods should be called before "createWindow"
    void setBgColorName(const char* colorName);
    void setFgColorName(const char* colorName);

    void setLineAttributes(
        unsigned int line_width, int line_style, 
        int cap_style, int join_style
    );

    void setLineWidth(unsigned int line_width);

    unsigned long allocateColor(const char *colorName);

    void setBackground(unsigned long bg);
    void setBackground(const char *colorName);
    void setForeground(unsigned long fg);
    void setForeground(const char *colorName);

    unsigned long getBackground() const { return m_bgPixel; }
    unsigned long getForeground() const { return m_fgPixel; }

    void fillRectangle(const I2Rectangle&, bool offscreen = false);
    void fillRectangle(const R2Rectangle&, bool offscreen = false);

    void fillPolygon(const R2Point* points, int numPoints, bool offscreen = false);
    void fillPolygon(const I2Point* points, int numPoints, bool offscreen = false);

    void fillEllipse(const I2Rectangle&, bool offscreen = false);
    void fillEllipse(const R2Rectangle&, bool offscreen = false);

    void redraw();
    void redrawRectangle(const R2Rectangle&);
    void redrawRectangle(const I2Rectangle&);

    void setWindowTitle(const char* title);

    I2Point map(const R2Point& p) const;
    I2Point map(double x, double y) const;
    int mapX(double x) const;
    int mapY(double y) const;

    R2Point invMap(const I2Point& p) const;

    void recalculateMap();

    // Font methods
	// Need to find out xcb_query_font_reply_t alternative
    xcb_font_t loadFont(const char* fontName, xcb_query_font_reply_t **fontStruct = 0);
    void unloadFont(xcb_font_t fontID);
    xcb_query_font_reply_t* queryFont(xcb_font_t fontID) const;
    void setFont(xcb_font_t fontID);

    // Depths supported
    bool supportsDepth24() const;
    bool supportsDepth32() const;
    bool supportsDepth(int d) const;

    // Work with offscreen buffer
    bool createOffscreenBuffer();
    void swapBuffers();

    // Callbacks:
    virtual void onExpose(xcb_expose_event_t* event);
    virtual void onResize(xcb_configure_notify_event_t* event); // event.xconfigure.width, height
    virtual void onKeyPress(xcb_key_press_event_t* event);
    virtual void onButtonPress(xcb_button_press_event_t* event);
    virtual void onButtonRelease(xcb_button_release_event_t* event);
    virtual void onMotionNotify(xcb_motion_notify_event_t* event);
    virtual void onCreateNotify(xcb_create_notify_event_t* event);
    virtual void onDestroyNotify(xcb_destroy_notify_event_t* event);
    virtual void onFocusIn(xcb_focus_in_event_t* event);
    virtual void onFocusOut(xcb_focus_out_event_t* event);

    // Message from Window Manager, such as "Close Window"
    virtual void onClientMessage(xcb_client_message_event_t* event);

    // This method is called from the base implementation of
    // method "onClientMessage". It allows a user application
    // to save all unsaved data and to do other operations before
    // closing a window, when a user pressed the closing box in the upper
    // right corner of a window. The application supplied method should return
    // "true" to close the window or "false" to leave the window open.
    virtual bool onWindowClosing();

    // Message loop
    static bool getNextEvent(xcb_generic_event_t*& e);
    static void dispatchEvent(xcb_generic_event_t* e);
    static void messageLoop(GWindow* = 0);

    // For dialog windows
    void doModal();

    // Some methods implementing X-primitives
    virtual void destroyWindow();
    void mapRaised();
    void raise();

private:
    int clip(const R2Point& p1, const R2Point& p2,
                   R2Point& c1,       R2Point& c2);
};

#endif
//
// End of file "gwindow.h"
