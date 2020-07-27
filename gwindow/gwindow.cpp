// File "gwindow.cpp"
// Simple graphic window based on XLib 
// 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>       
#include <sys/types.h>       
#include <unistd.h>
#include <assert.h>

#include "gwindow.h"

xcb_connection_t* GWindow::m_Connection = 0;
int				GWindow::m_Screen = 0;
xcb_screen_t*	GWindow::m_ScreenInfo = 0;
xcb_atom_t		GWindow::m_WMProtocolsAtom = 0;
xcb_atom_t		GWindow::m_WMDeleteWindowAtom = 0;

int        GWindow::m_NumWindows = 0;
int        GWindow::m_NumCreatedWindows = 0;
ListHeader GWindow::m_WindowList(
               &GWindow::m_WindowList, &GWindow::m_WindowList
           );

ListHeader GWindow::m_FontList(
               &GWindow::m_FontList, &GWindow::m_FontList
           );

void GWindow::messageLoop(GWindow* dialogWnd /* = 0 */) {
    xcb_generic_event_t *event;
    xcb_flush(m_Connection);
    while (
        m_NumCreatedWindows > 0 &&
        (dialogWnd == 0 || dialogWnd->m_Window != 0)
    ) {
        event = xcb_wait_for_event(m_Connection);
		if(event){
        	dispatchEvent(event);
		}
		else
			;// TODO
    }

    while (event = xcb_poll_for_event(m_Connection)) {
        dispatchEvent(event);
    }
}

void GWindow::dispatchEvent(xcb_generic_event_t* event) {
    GWindow* w;
//	printf("Event type: %d\n", event->response_type);
    if (event->response_type == XCB_EXPOSE) {
        printf("Expose!\n");
		xcb_expose_event_t *exposeEvent = (xcb_expose_event_t*)event;
		w = findWindow(exposeEvent->window);
		if(!w) return;
        if (w->m_BeginExposeSeries) {
            w->m_ClipRectangle.x = exposeEvent->x;
            w->m_ClipRectangle.y = exposeEvent->y;
            w->m_ClipRectangle.width = exposeEvent->width;
            w->m_ClipRectangle.height = exposeEvent->height;
            w->m_BeginExposeSeries = false;
        } else {
            // Add the current rectangle to the clip rectangle
            if (exposeEvent->x < w->m_ClipRectangle.x)
                w->m_ClipRectangle.x = exposeEvent->x;
            if (exposeEvent->y < w->m_ClipRectangle.y)
                w->m_ClipRectangle.y = exposeEvent->y;
            if (exposeEvent->x + exposeEvent->width >
                w->m_ClipRectangle.x + w->m_ClipRectangle.width) {
                w->m_ClipRectangle.width = exposeEvent->x +
                    exposeEvent->width - w->m_ClipRectangle.x;
            }
            if (exposeEvent->y + exposeEvent->height >
                w->m_ClipRectangle.y + w->m_ClipRectangle.height) {
                w->m_ClipRectangle.height = exposeEvent->y +
                    exposeEvent->height - w->m_ClipRectangle.y;
            }
        }
        if (exposeEvent->count == 0) {
            // Restrict a drawing to clip rectangle
            xcb_set_clip_rectangles(
                m_Connection, XCB_CLIP_ORDERING_UNSORTED,
			   	w->m_GC, 0, 0,                   // Clip origin
                1, &(w->m_ClipRectangle)
            );

            w->onExpose(exposeEvent);

            // Restore the clip region
            w->m_ClipRectangle.x = 0;
            w->m_ClipRectangle.y = 0;
            w->m_ClipRectangle.width = w->m_IWinRect.width();
            w->m_ClipRectangle.height = w->m_IWinRect.height();
            xcb_set_clip_rectangles(
                m_Connection, XCB_CLIP_ORDERING_UNSORTED,
			   	w->m_GC, 0, 0,                   // Clip origin
                1, &(w->m_ClipRectangle)
            );
            w->m_BeginExposeSeries = true;
        }
    } else if (event->response_type == XCB_KEY_PRESS) {
		xcb_key_press_event_t *keyPressEvent = (xcb_key_press_event_t*)event;
		w = findWindow(keyPressEvent->event);
		if(!w) return;
        w->onKeyPress(keyPressEvent);
    } else if (event->response_type == XCB_BUTTON_PRESS) {
		xcb_button_press_event_t *buttonPressEvent = (xcb_button_press_event_t*)event;
		w = findWindow(buttonPressEvent->event);
		if(!w) return;
        w->onButtonPress(buttonPressEvent);
    } else if (event->response_type == XCB_BUTTON_RELEASE) {
		xcb_button_release_event_t *buttonReleaseEvent = (xcb_button_release_event_t*)event;
		w = findWindow(buttonReleaseEvent->event);
		if(!w) return;
        w->onButtonRelease(buttonReleaseEvent);
    } else if (event->response_type == XCB_MOTION_NOTIFY) {
		xcb_motion_notify_event_t *motionNotifyEvent = (xcb_motion_notify_event_t*)event;
		w = findWindow(motionNotifyEvent->event);
		if(!w) return;
        w->onMotionNotify(motionNotifyEvent);
    } else if (event->response_type == XCB_CREATE_NOTIFY) {
		xcb_create_notify_event_t *createNotifyEvent = (xcb_create_notify_event_t*)event;
		w = findWindow(createNotifyEvent->window);
		if(!w) return;
        w->onCreateNotify(createNotifyEvent);
    } else if (event->response_type == XCB_DESTROY_NOTIFY) {
		xcb_destroy_notify_event_t *destroyNotifyEvent = (xcb_destroy_notify_event_t*)event;
		w = findWindow(destroyNotifyEvent->event);
		if(!w) return;
        w->onDestroyNotify(destroyNotifyEvent);

        GWindow* destroyedWindow = findWindow(destroyNotifyEvent->window);
        if (destroyedWindow != 0) {
            if (destroyedWindow->m_WindowCreated) {
                destroyedWindow->m_WindowCreated = false;
                m_NumCreatedWindows--;
            }
            // Exclude a window from the window list
            if (destroyedWindow->prev != 0) {
                destroyedWindow->prev->link(*(destroyedWindow->next));
                destroyedWindow->prev = 0;
                destroyedWindow->next = 0;
                m_NumWindows--;
            }
        }

    } else if (event->response_type == XCB_FOCUS_IN) {
		xcb_focus_in_event_t *focusInEvent = (xcb_focus_in_event_t*)event;
		w = findWindow(focusInEvent->event);
		if(!w) return;
        w->onFocusIn(focusInEvent);
    } else if (event->response_type == XCB_FOCUS_OUT) {
		xcb_focus_out_event_t *focusOutEvent = (xcb_focus_out_event_t*)event;
		w = findWindow(focusOutEvent->event);
		if(!w) return;
        w->onFocusOut(focusOutEvent);
    } else if (event->response_type == XCB_CONFIGURE_NOTIFY) {
		xcb_configure_notify_event_t *configureEvent = (xcb_configure_notify_event_t*)event;
		w = findWindow(configureEvent->window);
		if(!w) return;
        int newWidth = configureEvent->width;
        int newHeight = configureEvent->height;
        if (
            newWidth != w->m_IWinRect.width() ||
            newHeight != w->m_IWinRect.height()
        ) {
            w->m_IWinRect.setWidth(newWidth);
            w->m_IWinRect.setHeight(newHeight);
            w->recalculateMap();
            if (w->m_Pixmap != 0) {
                // Offscreen drawing is used
                int depth = m_ScreenInfo->root_depth;
                ::xcb_free_pixmap(m_Connection, w->m_Pixmap);
                w->m_Pixmap = xcb_generate_id(m_Connection);
				::xcb_create_pixmap(
                    m_Connection, depth, w->m_Pixmap,
					w->m_Window, newWidth, newHeight
                );
                xcb_flush(m_Connection);
				// TODO: pixmap might not be ready before onResize
            }
            w->onResize(configureEvent);
            w->redraw();
        }
    } else if (event->response_type == XCB_CLIENT_MESSAGE) { // Window closing, etc.
		printf("client message!\n");
		xcb_client_message_event_t *clientMessageEvent = (xcb_client_message_event_t*)event;
		w = findWindow(clientMessageEvent->window);
		if(!w) return;
        w->onClientMessage(clientMessageEvent);
    }
    xcb_flush(m_Connection);
	free(event);
}

void GWindow::doModal() {
    messageLoop(this);
}

GWindow* GWindow::findWindow(xcb_window_t w) {
    ListHeader* p = m_WindowList.next;
    int i = 0;
    while (i < m_NumWindows && p != &m_WindowList) {
        if (((GWindow*) p)->m_Window == w) {
            return ((GWindow *)p);
        }
        p = p->next;
        i++;
    }
    return 0;
}

GWindow::GWindow():
    m_Window(0),
    m_Pixmap(0),
    m_GC(0),
    m_WindowPosition(0, 0),
    m_IWinRect(I2Point(0, 0), 300, 200),    // Some arbitrary values
    m_RWinRect(
        R2Point(0., 0.), 
        300., 200.
    ),
    m_ICurPos(0, 0),
    m_RCurPos(0., 0.),
    m_xcoeff(1.),
    m_ycoeff(1.),
    m_WindowCreated(false),
    m_bgPixel(0),
    m_fgPixel(0),
    m_bgColorName(0),
    m_fgColorName(0),
    m_BorderWidth(DEFAULT_BORDER_WIDTH),
    m_BeginExposeSeries(true)
{
    strcpy(m_WindowTitle, "Graphic Window");
}

GWindow::GWindow(
    const I2Rectangle& frameRect,
    const char* title /* = 0 */
):
    m_Window(0),
    m_Pixmap(0),
    m_GC(0),
    m_WindowPosition(frameRect.left(), frameRect.top()),
    m_IWinRect(I2Point(0, 0), frameRect.width(), frameRect.height()),
    m_RWinRect(),
    m_ICurPos(0, 0),
    m_RCurPos(0., 0.),
    m_xcoeff(1.),
    m_ycoeff(1.),
    m_WindowCreated(false),
    m_bgPixel(0),
    m_fgPixel(0),
    m_bgColorName(0),
    m_fgColorName(0),
    m_BorderWidth(DEFAULT_BORDER_WIDTH)
{
    GWindow(            // Call another constructor
        frameRect,
        R2Rectangle(
            R2Point(0., 0.),
            (double) frameRect.width(), (double) frameRect.height()
        ),
        title
    );
}

GWindow::GWindow(
    const I2Rectangle& frameRect, 
    const R2Rectangle& coordRect,
    const char* title /* = 0 */
):
    m_Window(0),
    m_Pixmap(0),
    m_GC(0),
    m_WindowPosition(frameRect.left(), frameRect.top()),
    m_IWinRect(I2Point(0, 0), frameRect.width(), frameRect.height()),
    m_RWinRect(coordRect),
    m_ICurPos(0, 0),
    m_RCurPos(0., 0.),
    m_xcoeff(1.),
    m_ycoeff(1.),
    m_WindowCreated(false),
    m_bgPixel(0),
    m_fgPixel(0),
    m_bgColorName(0),
    m_fgColorName(0),
    m_BorderWidth(DEFAULT_BORDER_WIDTH)
{
    if (title == 0) {
        strcpy(m_WindowTitle, "Graphic Window");
    } else {
        strncpy(m_WindowTitle, title, 127);
        m_WindowTitle[127] = 0;
    }

    if (m_IWinRect.width() == 0) {
        m_IWinRect.setWidth(1);
        m_RWinRect.setWidth(1);
    }
    if (m_IWinRect.height() == 0) {
        m_IWinRect.setHeight(1);
        m_RWinRect.setHeight(1);
    }

    m_xcoeff = double(frameRect.width()) / coordRect.width();
    m_ycoeff = double(frameRect.height()) / coordRect.height();
    m_ICurPos = map(m_RCurPos);
}

void GWindow::createWindow(
    GWindow* parentWindow /* = 0 */,                // parent window
    int borderWidth       /* = DEFAULT_BORDER_WIDTH */,
    xcb_window_class_t wndClass /* = InputOutput */,      // InputOnly, CopyFromParent
    xcb_visualid_t visual        /* = CopyFromParent */,
    uint32_t attributesValueMask /* = 0 */,    // which attr. are defined
    void* attributes  /* = 0 */     // attributes structure
) {
    // Include a window in the head of the window list
    link(*(m_WindowList.next));
    m_WindowList.link(*this);

    m_NumWindows++;
    m_NumCreatedWindows++;

    // Open a display, if necessary
    if (m_Connection == 0)
        initX();

    // Create window and map it

    if (m_IWinRect.left() != 0 || m_IWinRect.top() != 0) {
        int x = m_IWinRect.left();
        int y = m_IWinRect.top();
        int w = m_IWinRect.width();
        int h = m_IWinRect.height();
        m_WindowPosition.x += x;
        m_WindowPosition.y += y;
        m_IWinRect = I2Rectangle(0, 0, w, h);
    }

    if (m_bgColorName != 0) {
        xcb_alloc_named_color_cookie_t cookie = xcb_alloc_named_color_unchecked(
                                                    m_Connection,
                                                    m_ScreenInfo->default_colormap,
                                                    strlen(m_bgColorName),
                                                    m_bgColorName
                                                );
        xcb_alloc_named_color_reply_t *bg = xcb_alloc_named_color_reply(
                                                m_Connection, cookie, NULL
                                            );
        m_bgPixel = bg->pixel;
    } else {
        m_bgPixel = m_ScreenInfo->white_pixel;
    }

    if (m_fgColorName != 0) {
        xcb_alloc_named_color_cookie_t cookie = xcb_alloc_named_color_unchecked(
                                                    m_Connection,
                                                    m_ScreenInfo->default_colormap,
                                                    strlen(m_fgColorName),
                                                    m_fgColorName
                                                );
        xcb_alloc_named_color_reply_t *fg = xcb_alloc_named_color_reply(
                                                m_Connection, cookie, NULL
                                            );
        m_fgPixel = fg->pixel;
    } else {
        m_fgPixel = m_ScreenInfo->black_pixel;
    }

    m_BorderWidth = borderWidth;

    xcb_window_t parent;
    if (parentWindow != 0 && parentWindow->m_Window != 0)
        parent = parentWindow->m_Window;
    else
        parent = m_ScreenInfo->root;

    m_Window = xcb_generate_id(m_Connection);
    xcb_create_window(
        m_Connection,
        XCB_COPY_FROM_PARENT,
        m_Window,
        parent,
        m_WindowPosition.x,
        m_WindowPosition.y,
        m_IWinRect.width(),
        m_IWinRect.height(),
        m_BorderWidth,
        wndClass,
        visual,
        attributesValueMask,
        attributes
    );

    m_WindowCreated = true;

    xcb_change_property(m_Connection, XCB_PROP_MODE_REPLACE, m_Window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(m_WindowTitle), m_WindowTitle);
    xcb_change_property(m_Connection, XCB_PROP_MODE_REPLACE, m_Window,
                        XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8, strlen(m_WindowTitle), m_WindowTitle);

    long eventMask =  
       XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
       | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_POINTER_MOTION
       | XCB_EVENT_MASK_STRUCTURE_NOTIFY       // For resize event
       | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
       | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes(
        m_Connection,
        m_Window,
        XCB_CW_EVENT_MASK,
        &eventMask
    );


    uint32_t gcValues[2] = {m_fgPixel, m_bgPixel};
    m_GC = xcb_generate_id(m_Connection);
    xcb_create_gc(m_Connection, m_GC, m_Window, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gcValues);

    xcb_clear_area(m_Connection, 0, m_Window, 0, 0, 0, 0);
    xcb_map_window(m_Connection, m_Window);
    uint32_t stackTop = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(m_Connection, m_Window, XCB_CONFIG_WINDOW_STACK_MODE, &stackTop);

    xcb_change_property(m_Connection, XCB_PROP_MODE_REPLACE, m_Window,
                m_WMProtocolsAtom, XCB_ATOM_ATOM, 32, 1, &m_WMDeleteWindowAtom);

    // printf("In createWindow: m_Window = %d\n", (int) m_Window);
}

void GWindow::setWindowTitle(const char* title) {
    strncpy(m_WindowTitle, title, 127);
    m_WindowTitle[127] = 0;

    if (m_WindowCreated);
//        XStoreName(m_Connection, m_Window, m_WindowTitle);
}

void GWindow::setBgColorName(const char* colorName) {
    if (m_WindowCreated)
        setBackground(colorName);
    else
        m_bgColorName = colorName;
}

void GWindow::setFgColorName(const char* colorName) {
    if (m_WindowCreated)
        setForeground(colorName);
    else
        m_fgColorName = colorName;
}

void GWindow::createWindow(
    const I2Rectangle& frameRect, 
    const char* title     /* = 0 */,
    GWindow *parentWindow /* = 0 */,
    int borderWidth       /* = DEFAULT_BORDER_WIDTH */
) {
    if (title == 0) {
        strcpy(m_WindowTitle, "Graphic Window");
    } else {
        strncpy(m_WindowTitle, title, 127);
        m_WindowTitle[127] = 0;
    }

    m_WindowPosition.x = frameRect.left();
    m_WindowPosition.y = frameRect.top();

    m_IWinRect.setLeft(0);
    m_IWinRect.setTop(0);
    m_IWinRect.setWidth(frameRect.width());
    m_IWinRect.setHeight(frameRect.height());

    m_RWinRect.setLeft(0.);
    m_RWinRect.setBottom(0.);
    m_RWinRect.setWidth((double) frameRect.width());
    m_RWinRect.setHeight((double) frameRect.height());

    if (m_IWinRect.width() == 0) {
        m_IWinRect.setWidth(1);
        m_RWinRect.setWidth(1);
    }
    if (m_IWinRect.height() == 0) {
        m_IWinRect.setHeight(1);
        m_RWinRect.setHeight(1);
    }
    m_xcoeff = double(m_IWinRect.width()) / m_RWinRect.width();
    m_ycoeff = double(m_IWinRect.height()) / m_RWinRect.height();
    m_ICurPos = map(m_RCurPos);

    createWindow(parentWindow, borderWidth);
}

void GWindow::createWindow(
    const I2Rectangle& frameRect, 
    const R2Rectangle& coordRect,
    const char* title     /* = 0 */,
    GWindow* parentWindow /* = 0 */,
    int borderWidth       /* = DEFAULT_BORDER_WIDTH */
) {
    if (title == 0) {
        strcpy(m_WindowTitle, "Graphic Window");
    } else {
        strncpy(m_WindowTitle, title, 127);
        m_WindowTitle[127] = 0;
    }

    m_WindowPosition.x = frameRect.left();
    m_WindowPosition.y = frameRect.top();

    m_IWinRect.setLeft(0);
    m_IWinRect.setTop(0);
    m_IWinRect.setWidth(frameRect.width());
    m_IWinRect.setHeight(frameRect.height());

    m_RWinRect = coordRect;

    if (m_IWinRect.width() == 0) {
        m_IWinRect.setWidth(1);
    }
    if (m_IWinRect.height() == 0) {
        m_IWinRect.setHeight(1);
    }
    //... if (m_RWinRect.width() == 0.) {
    if (fabs(m_RWinRect.width()) <= R2GRAPH_EPSILON) {
        m_RWinRect.setWidth(1.);
    }
    //... if (m_RWinRect.height() == 0.) {
    if (fabs(m_RWinRect.height()) <= R2GRAPH_EPSILON) {
        m_RWinRect.setHeight(1.);
    }
    m_xcoeff = double(m_IWinRect.width()) / m_RWinRect.width();
    m_ycoeff = double(m_IWinRect.height()) / m_RWinRect.height();
    m_ICurPos = map(m_RCurPos);

    createWindow(parentWindow, borderWidth);
}

GWindow::~GWindow() {
    if (m_WindowCreated) {
        destroyWindow();        // Destroy window
        m_WindowCreated = false;
    }

    // Exclude a window from the window list
    if (prev != 0) {
        prev->link(*next);
        prev = 0;
        next = 0;
        m_NumWindows--;
    }
}

void GWindow::destroyWindow() {

    if (!m_WindowCreated)
        return;
    m_WindowCreated = false;
    m_NumCreatedWindows--;

    if (m_GC != 0) {
        xcb_free_gc(m_Connection, m_GC);
        m_GC = 0;
    }
    if (m_Pixmap != 0) {
        xcb_free_pixmap(m_Connection, m_Pixmap);
        m_Pixmap = 0;
    }
    if (m_Window != 0) {
        xcb_destroy_window(m_Connection, m_Window);
        m_Window = 0;
    }
}

bool GWindow::initX() {
	
	xcb_intern_atom_cookie_t WMProtocolsCookie, WMDeleteWindowCookie;
	xcb_intern_atom_reply_t *reply;
	xcb_generic_error_t *atomError;

    m_Connection = xcb_connect(NULL, &m_Screen);
    if (m_Connection == 0) {
        perror("Cannot open display");
        return false;
    }

    // For interconnection with Window Manager
    WMProtocolsCookie = xcb_intern_atom(
        GWindow::m_Connection,
		0,
		strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS"
    );
    WMDeleteWindowCookie = xcb_intern_atom(
        GWindow::m_Connection,
		0,
		strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW"
    );

	reply = xcb_intern_atom_reply(m_Connection,
			WMProtocolsCookie, &atomError);
	if(reply){
		m_WMProtocolsAtom = reply->atom;
		free(reply);
	}
	else{
		fprintf(stderr, "Failed to get WM_Protocols atom: xcb error %d\n", atomError->error_code);
		return false;
	}

	reply = xcb_intern_atom_reply(m_Connection,
			WMDeleteWindowCookie, &atomError);
	if(reply){
		m_WMDeleteWindowAtom = reply->atom;
		free(reply);
	}
	else{
		fprintf(stderr, "Failed to get WM_DELETE_WINDOW atom: xcb error %d\n", atomError->error_code);
		return false;
	}

	m_ScreenInfo = NULL;
    int tempScreen = m_Screen;
	for(xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(m_Connection));
        iter.rem; --tempScreen, xcb_screen_next(&iter)){
		if(tempScreen == 0){
			m_ScreenInfo = iter.data;
		}
	}
	if(!m_ScreenInfo){
		fprintf(stderr, "Failed to get screen info");
	}
    return true;
}

void GWindow::closeX() {
    if (m_Connection == 0)
        return;
	releaseFonts();

    xcb_disconnect(m_Connection);
    m_Connection = 0;
}

int GWindow::screenMaxX() {
    if (m_Connection == 0)
        initX();
    return m_ScreenInfo->width_in_pixels;
}

int GWindow::screenMaxY() {
    if (m_Connection == 0)
        initX();
    return m_ScreenInfo->height_in_pixels;
}

void GWindow::drawFrame() {
}

void GWindow::moveTo(const I2Point& p) {
    m_ICurPos = p;
    m_RCurPos = invMap(m_ICurPos);
}

void GWindow::moveTo(const R2Point& p) {
    m_RCurPos = p;
    m_ICurPos = map(m_RCurPos);
}

void GWindow::moveTo(int x, int y) {
    moveTo(I2Point(x, y));
}

void GWindow::moveTo(double x, double y) {
    moveTo(R2Point(x, y));
}

void GWindow::setCoordinates(
    double xmin, double ymin, double width, double height
) {
    setCoordinates(R2Rectangle(xmin, ymin, width, height));
}

void GWindow::setCoordinates(
    const R2Point& leftBottom, const R2Point& rightTop
) {
    setCoordinates(
        R2Rectangle(
            leftBottom, 
            rightTop.x - leftBottom.x,
            rightTop.y - leftBottom.y
        )
    );
}

void GWindow::setCoordinates(const R2Rectangle& coordRect) {
    m_RWinRect = coordRect;
    //... if (m_RWinRect.width() == 0) {
    if (fabs(m_RWinRect.width()) <= R2GRAPH_EPSILON) {
        m_RWinRect.setWidth(1);
    }
    //... if (m_RWinRect.height() == 0) {
    if (fabs(m_RWinRect.height()) <= R2GRAPH_EPSILON) {
        m_RWinRect.setHeight(1);
    }
    m_xcoeff = double(m_IWinRect.width()) / double(m_RWinRect.width());
    m_ycoeff = double(m_IWinRect.height()) / double(m_RWinRect.height());
}

void GWindow::drawAxes(
    const char* axesColor /* = 0 */,
    bool drawGrid         /* = false */,
    const char* gridColor /* = 0 */,
    bool offscreen        /* = false */
) {
    // Af first, draw a grid
    if (drawGrid) {
        if (gridColor != 0)
            setForeground(gridColor);
        // Vertical grid
        int xmin = (int) ceil(m_RWinRect.left());
        int xmax = (int) floor(m_RWinRect.right());
        int i;
        for (i = xmin; i <= xmax; i++) {
            if (i == 0)
                continue;
            drawLine(
                R2Point((double) i, m_RWinRect.bottom()),
                R2Point((double) i, m_RWinRect.top()),
                offscreen
            );
        }
        int ymin = (int) ceil(m_RWinRect.bottom());
        int ymax = (int) floor(m_RWinRect.top());
        for (i = ymin; i <= ymax; i++) {
            if (i == 0)
                continue;
            drawLine(
                R2Point(m_RWinRect.left(), (double) i),
                R2Point(m_RWinRect.right(), (double) i),
                offscreen
            );
        }
    }

    if (axesColor != 0)
        setForeground(axesColor);
    drawLine(R2Point(getXMin(), 0.), R2Point(getXMax(), 0.), offscreen);
    drawLine(R2Point(0., getYMin()), R2Point(0., getYMax()), offscreen);

    // Scale
    drawLine(R2Point(1., -0.1), R2Point(1., 0.1), offscreen);
    drawLine(R2Point(-0.1, 1.), R2Point(0.1, 1.), offscreen);

    double w = m_RWinRect.width();
    double h = m_RWinRect.height();
    drawString(R2Point(getXMin() + w * 0.9, -h*0.06), "x", 1, offscreen);
    drawString(R2Point(w * 0.03, getYMin() + h*0.9), "y", 1, offscreen);
}

void GWindow::moveRel(const I2Vector& v) {
    moveTo(m_ICurPos + v);
}

void GWindow::moveRel(const R2Vector& v) {
    moveTo(m_RCurPos + v);
}

void GWindow::moveRel(int dx, int dy) {
    moveRel(I2Vector(dx, dy));
}

void GWindow::moveRel(double dx, double dy) {
    moveRel(R2Vector(dx, dy));
}

void GWindow::drawLineTo(const I2Point& p, bool offscreen /* = false */) {
    drawLine(m_ICurPos, p, offscreen);
    moveTo(p);
}

void GWindow::drawLineTo(int x, int y, bool offscreen /* = false */) {
    drawLineTo(I2Point(x, y), offscreen);
}

void GWindow::drawLineTo(const R2Point& p, bool offscreen /* = false */) {
    drawLine(m_RCurPos, p, offscreen);
    moveTo(p);
}

void GWindow::drawLineTo(double x, double y, bool offscreen /* = false */) {
    drawLineTo(R2Point(x, y), offscreen);
}

void GWindow::drawLineRel(const I2Vector& v, bool offscreen /* = false */) {
    drawLineTo(m_ICurPos + v, offscreen);
}

void GWindow::drawLineRel(const R2Vector& v, bool offscreen /* = false */) {
    drawLineTo(m_RCurPos + v, offscreen);
}

void GWindow::drawLineRel(int dx, int dy, bool offscreen /* = false */) {
    drawLineRel(I2Vector(dx, dy), offscreen);
}

void GWindow::drawLineRel(double dx, double dy, bool offscreen /* = false */) {
    drawLineRel(R2Vector(dx, dy), offscreen);
}

void GWindow::drawLine(
    const I2Point& p1, const I2Point& p2, bool offscreen /* = false */
) {
    //... drawLine(invMap(p1), invMap(p2));
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    if (
        abs(p1.x) < SHRT_MAX && abs(p1.y) < SHRT_MAX &&
        abs(p2.x) < SHRT_MAX && abs(p2.y) < SHRT_MAX
    )
    {
        xcb_point_t xp1, xp2;
        xp1.x = p1.x; xp1.y = p1.y;
        xp2.x = p2.x; xp2.y = p2.y;
        xcb_point_t points[2] = {xp1, xp2};
        ::xcb_poly_line(
            m_Connection,
            XCB_COORD_MODE_ORIGIN,
            draw,
            m_GC,
            2, points
        );
    } else {
        R2Point c1, c2;
        if (
            R2Rectangle(
                m_IWinRect.left(), m_IWinRect.top(),
                m_IWinRect.width(), m_IWinRect.height()
            ).clip(
                R2Point(p1.x, p1.y), R2Point(p1.x, p1.y),
                c1, c2
            )
        ) {
            xcb_point_t xp1, xp2;
            xp1.x = (int)(c1.x + 0.5); xp1.y = (int)(c1.y + 0.5);
            xp2.x = (int)(c2.x + 0.5); xp2.y = (int)(c2.y + 0.5);
            xcb_point_t points[2] = {xp1, xp2};
            ::xcb_poly_line(
                m_Connection,
                XCB_COORD_MODE_ORIGIN,
                draw,
                m_GC,
                2, points
            );
        }
    }
    moveTo(invMap(p2));
}

void GWindow::drawLine(
    const I2Point& p,  const I2Vector& v, bool offscreen /* = false */
) {
    drawLine(p, p+v, offscreen);
}

void GWindow::drawLine(
    int x1, int y1, int x2, int y2, bool offscreen /* = false */
) {
    drawLine(I2Point(x1, y1), I2Point(x2, y2), offscreen);
}

void GWindow::drawLine(
    const R2Point& p1, const R2Point& p2, bool offscreen /* = false */
) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    R2Point c1, c2;
    if (m_RWinRect.clip(p1, p2, c1, c2)) {
        I2Point ip1 = map(c1), ip2 = map(c2);

        xcb_point_t xp1, xp2;
        xp1.x = ip1.x; xp1.y = ip1.y;
        xp2.x = ip2.x; xp2.y = ip2.y;
        xcb_point_t points[2] = {xp1, xp2};
        ::xcb_poly_line(
            m_Connection,
            XCB_COORD_MODE_ORIGIN,
            draw,
            m_GC,
            2, points
        );
    }
}

void GWindow::drawLine(
    const R2Point& p,  const R2Vector& v, bool offscreen /* = false */
) {
    drawLine(p, p+v, offscreen);
}

void GWindow::drawLine(
    double x1, double y1, double x2, double y2, bool offscreen /* = false */
) {
    drawLine(R2Point(x1, y1), R2Point(x2, y2), offscreen);
}

void GWindow::fillRectangle(const I2Rectangle& r, bool offscreen /* = false */) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    xcb_rectangle_t xrectangle;
    xrectangle.height = r.height(); xrectangle.width = r.width();
    xrectangle.x = r.left(); xrectangle.y = r.top();
    xcb_poly_fill_rectangle(m_Connection, draw, m_GC, 1, &xrectangle);
}

void GWindow::fillRectangle(const R2Rectangle& r, bool offscreen /* = false */) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    xcb_rectangle_t xrectangle;
    xrectangle.height = rightBottom.y - leftTop.y;
    xrectangle.width = rightBottom.x - leftTop.y;
    xrectangle.x = leftTop.x; xrectangle.y = leftTop.y;
    xcb_poly_fill_rectangle(m_Connection, draw, m_GC, 1, &xrectangle);
}

void GWindow::fillPolygon(
    const R2Point* points, int numPoints, bool offscreen /* = false */
) {
    if (numPoints <= 2)
        return;

    I2Point* pnt = new I2Point[numPoints];
    for (int i = 0; i < numPoints; ++i) {
        pnt[i] = map(points[i]);
    }
    fillPolygon(pnt, numPoints, offscreen);
    delete[] pnt;
}

void GWindow::fillPolygon(
    const I2Point* points, int numPoints, bool offscreen /* = false */
) {
    if (numPoints <= 2)
        return;

    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;


    xcb_point_t* pnt = new xcb_point_t[numPoints];
    pnt[0].x = (short) points[0].x;
    pnt[0].y = (short) points[0].y;
    for (int i = 1; i < numPoints; ++i) {
        pnt[i].x = (short) points[i].x;
        pnt[i].y = (short) points[i].y;
    }
    xcb_fill_poly(m_Connection, draw, m_GC, XCB_POLY_SHAPE_CONVEX,
                  XCB_COORD_MODE_ORIGIN, numPoints, pnt);
    delete[] pnt;
}

void GWindow::fillEllipse(const I2Rectangle& r, bool offscreen /* = false */) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    xcb_arc_t xarc;
    xarc.x = r.left(); xarc.y = r.top();
    xarc.width = r.width(); xarc.height = r.height();
    xarc.angle1 = 0; xarc.angle2 = 360 << 6;
    xcb_poly_fill_arc(m_Connection, draw, m_GC, 1, &xarc);
}

void GWindow::fillEllipse(const R2Rectangle& r, bool offscreen /* = false */) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    xcb_arc_t xarc;
    xarc.x = leftTop.x; xarc.y = leftTop.y;
    xarc.width = rightBottom.x - leftTop.x;
    xarc.height = rightBottom.y - leftTop.y;
    xarc.angle1 = 0; xarc.angle2 = 360 << 6;
    xcb_poly_fill_arc(m_Connection, draw, m_GC, 1, &xarc);
}

void GWindow::redraw() {
    if (!m_WindowCreated)
        return;
    redrawRectangle(
        I2Rectangle(0, 0, m_IWinRect.width(), m_IWinRect.height())
    );
}

void GWindow::redrawRectangle(const I2Rectangle& r) {
    xcb_rectangle_t rect;
    rect.x = (short) r.left(); rect.y = (short) r.top();
    rect.width = (unsigned short) r.width(); 
    rect.height = (unsigned short) r.height();
    xcb_set_clip_rectangles(m_Connection, XCB_CLIP_ORDERING_UNSORTED,
                        m_GC, 0, 0, 1, &rect);

    xcb_expose_event_t *e = (xcb_expose_event_t*)malloc(sizeof(xcb_expose_event_t));
    memset(e, 0, sizeof(xcb_expose_event_t));
    e->response_type = XCB_EXPOSE;
    e->window = m_Window;
    e->x = r.left();
    e->y = r.top();
    e->width = r.width();
    e->height = r.height();
    e->count = 0;
	xcb_send_event(m_Connection, 0, m_Window, XCB_EVENT_MASK_EXPOSURE, (char*)e);
	xcb_flush(m_Connection);
}

void GWindow::redrawRectangle(const R2Rectangle& r) {
    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));
    redrawRectangle(
        I2Rectangle(
            leftTop,
            rightBottom.x - leftTop.x,
            rightBottom.y - leftTop.y
        )
    );
}

void GWindow::drawString(
    int x, int y, const char* str, int len /* = (-1) */,
    bool offscreen /* = false */
) {
    int l = len;
    if (l < 0)
        l = strlen(str);

    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    xcb_image_text_8(m_Connection, l, draw, m_GC, x, y, str);
}

void GWindow::drawString(
    const I2Point& p, const char* str, int len, bool offscreen /* = false */
) {
    drawString(p.x, p.y, str, len, offscreen);
}

void GWindow::drawString(
    const R2Point& p, const char* str, int len, bool offscreen /* = false */
) {
    drawString(map(p), str, len, offscreen);
}

void GWindow::drawString(
    double x, double y, const char* str, int len, bool offscreen /* = false */
) {
    drawString(map(R2Point(x, y)), str, len, offscreen);
}

unsigned long GWindow::allocateColor(const char* colorName) {
    xcb_alloc_named_color_reply_t *c;
    xcb_alloc_named_color_cookie_t cookie = 
        xcb_alloc_named_color_unchecked(
            m_Connection,
            m_ScreenInfo->default_colormap,
            strlen(colorName),
            colorName
    );
    c = xcb_alloc_named_color_reply(m_Connection, cookie, NULL);
    return c->pixel;
}

void GWindow::setBackground(unsigned long bg) {
    uint32_t tempBg = bg;
    xcb_change_gc(m_Connection, m_GC, XCB_GC_BACKGROUND, &bg);
    m_bgPixel = bg;
}

void GWindow::setBackground(const char* colorName) {
    unsigned long bgPixel = allocateColor(colorName);
    xcb_change_gc(m_Connection, m_GC, XCB_GC_BACKGROUND, &bgPixel);
    m_bgPixel = bgPixel;
}

void GWindow::setForeground(unsigned long fg) {
    uint32_t tempFg = fg;
    xcb_change_gc(m_Connection, m_GC, XCB_GC_FOREGROUND, &fg);
    m_fgPixel = fg;
}

void GWindow::setForeground(const char* colorName) {
    unsigned long fgPixel = allocateColor(colorName);
    xcb_change_gc(m_Connection, m_GC, XCB_GC_FOREGROUND, &fgPixel);
    m_fgPixel = fgPixel;
}

I2Point GWindow::map(const R2Point& p) const {
    return I2Point(
        mapX(p.x), mapY(p.y)
    );
}

I2Point GWindow::map(double x, double y) const {
    return I2Point(
        mapX(x), mapY(y)
    );
}

int GWindow::mapX(double x) const {
    return int(
        (x - m_RWinRect.left())*m_xcoeff
    );
}

int GWindow::mapY(double y) const {
    return int(
        (m_RWinRect.top() - y)*m_ycoeff
    );
}

R2Point GWindow::invMap(const I2Point& p) const {
    return R2Point(
        double(p.x - m_IWinRect.left()) *
        m_RWinRect.width() / m_IWinRect.width()
        + m_RWinRect.left(),

        double(m_IWinRect.bottom() - p.y) *
        m_RWinRect.height() / m_IWinRect.height()
        + m_RWinRect.bottom()
    );
}

void GWindow::onExpose(xcb_expose_event_t*) {

}

void GWindow::onResize(xcb_configure_notify_event_t*) {

}

void GWindow::onKeyPress(xcb_key_press_event_t*) {

}

void GWindow::onButtonPress(xcb_button_press_event_t*) {

}

void GWindow::onButtonRelease(xcb_button_release_event_t*) {

}

void GWindow::onMotionNotify(xcb_motion_notify_event_t*) {

}

void GWindow::onCreateNotify(xcb_create_notify_event_t*) {

}

void GWindow::onDestroyNotify(xcb_destroy_notify_event_t*) {

}

void GWindow::onFocusIn(xcb_focus_in_event_t*) {

}

void GWindow::onFocusOut(xcb_focus_out_event_t*) {

}

void GWindow::recalculateMap() {
    if (m_IWinRect.width() == 0)
        m_IWinRect.setWidth(1);
    if (m_IWinRect.height() == 0)
        m_IWinRect.setHeight(1);

    m_xcoeff = double(m_IWinRect.width()) / m_RWinRect.width();
    m_ycoeff = double(m_IWinRect.height()) / m_RWinRect.height();
    m_ICurPos = map(m_RCurPos);
}

xcb_font_t GWindow::loadFont(const char* fontName, xcb_query_font_reply_t **font_struct) {
    xcb_font_t fid = xcb_generate_id(m_Connection);
    xcb_void_cookie_t fontCookie = xcb_open_font_checked(m_Connection, fid, strlen(fontName), fontName);
    xcb_request_check(m_Connection, fontCookie);
    xcb_query_font_cookie_t cookie = 
        xcb_query_font(m_Connection, fid);
    xcb_query_font_reply_t *fStruct = xcb_query_font_reply(m_Connection, cookie, NULL);
    if (fStruct == NULL)
        return 0;
    xcb_font_t xcb_font_t = xcb_generate_id(m_Connection);
    if (font_struct != 0) {
        *font_struct = fStruct;
    }
    addFontDescriptor(fid, fStruct);
    return fid;
}

void GWindow::unloadFont(xcb_font_t fontID) {
    FontDescriptor* fd = findFont(fontID);
    if (fd == 0) {
        xcb_close_font(m_Connection, fontID);
    } else {
        xcb_close_font(m_Connection, fontID);
        free(fd->font_info);
        removeFontDescriptor(fd);
    }
}

xcb_query_font_reply_t* GWindow::queryFont(xcb_font_t fontID) const {
    // Normally, the xcb_font_t must be already loaded by xcb_open_font
    // (with GWindow::loadFont).
    FontDescriptor* fd = findFont(fontID);
    if (fd != 0) {
        return fd->font_info;
    } else {
        // Should not come here!
        fontID = xcb_generate_id(m_Connection);
        xcb_query_font_cookie_t cookie =
            xcb_query_font(m_Connection, fontID);
        return xcb_query_font_reply(m_Connection, cookie, NULL);
    }
}

void GWindow::setFont(xcb_font_t fontID) {
    xcb_font_t tempFontID = fontID;
    xcb_change_gc(m_Connection, m_GC, XCB_GC_FONT, &tempFontID);
}

// Message from Window Manager, such as "Close Window"
void GWindow::onClientMessage(xcb_client_message_event_t *event) {
    if (
        event->type == m_WMProtocolsAtom &&
        (xcb_atom_t) event->data.data32[0] == m_WMDeleteWindowAtom
    ) {
        if (onWindowClosing()) {
            destroyWindow();
        }
    }
}

// This method is called from the base implementation of
// method "onClientMessage". It allows a user application
// to save all unsaved data and to do other operations before
// closing a window, when a user pressed the closing box in the upper
// right corner of a window. The application supplied method should return
// "true" to close the window or false to leave the window open.
bool GWindow::onWindowClosing() {
    return true;        // Close the window
}

void GWindow::setLineAttributes(
    unsigned int line_width, int line_style,
    int cap_style, int join_style
) {
    unsigned int styles[4] = {line_width, line_style, cap_style, join_style};
    xcb_change_gc(m_Connection, m_GC,
        XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE,
        styles
    );
}

void GWindow::setLineWidth(unsigned int line_width) {
    unsigned int tempLineWidth = line_width;
    unsigned long valuemask = XCB_GC_LINE_WIDTH;
    xcb_change_gc(m_Connection, m_GC, valuemask, &tempLineWidth);
}

void GWindow::drawEllipse(const I2Rectangle& r, bool offscreen /* = false */) {
    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    xcb_arc_t xarc;
    xarc.x = r.left(); xarc.y = r.top();
    xarc.width = r.width(); xarc.height = r.height();
    xarc.angle1 = 0; xarc.angle2 = 360 << 6;
    xcb_poly_arc(m_Connection, draw, m_GC, 1, &xarc);

}

void GWindow::drawEllipse(const R2Rectangle& r, bool offscreen /* = false */) {
    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    xcb_drawable_t draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    xcb_arc_t xarc;
    xarc.x = leftTop.x; xarc.y = leftTop.y;
    xarc.width = rightBottom.x - leftTop.x;
    xarc.height = rightBottom.y - leftTop.y;
    xarc.angle1 = 0; xarc.angle2 = 360 << 6;
    xcb_poly_arc(m_Connection, draw, m_GC, 1, &xarc);
}

void GWindow::drawCircle(
    const I2Point& center, int radius, bool offscreen /* = false */
) {
    drawEllipse(
        I2Rectangle(
            center - I2Vector(radius, radius),
            2*radius, 2*radius
        ),
        offscreen
    );
}

void GWindow::drawCircle(
    const R2Point& center, double radius, bool offscreen /* = false */
) {
    drawEllipse(
        R2Rectangle(
            center - R2Vector(radius, radius),
            2.*radius, 2.*radius
        ),
        offscreen
    );
}

bool GWindow::supportsDepth(int d) const {
    if (m_Connection == 0)
        return false;
    for(xcb_depth_iterator_t iter = xcb_screen_allowed_depths_iterator(m_ScreenInfo);
        iter.rem; xcb_depth_next(&iter)){
        if (iter.data->depth == d) {
            return true;
        }
        

    }
    return false;
}

bool GWindow::supportsDepth24() const {
    return supportsDepth(24);
}

bool GWindow::supportsDepth32() const {
    return supportsDepth(32);
}

bool GWindow::createOffscreenBuffer() {
    if (m_Connection == 0)
        return false;
    int depth = m_ScreenInfo->root_depth;
    m_Pixmap = xcb_generate_id(m_Connection);
    ::xcb_create_pixmap(
        m_Connection, m_Pixmap, depth, m_Window,
        m_IWinRect.width(), m_IWinRect.height()
    );

    return (m_Pixmap > 0);
}

void GWindow::swapBuffers() {
    if (m_Pixmap > 0) {
        xcb_copy_area(m_Connection, m_Pixmap, m_Window, m_GC,
            0, 0, 0, 0,
            m_IWinRect.width(), m_IWinRect.height()
        );
    }
}

// Work with the list of xcb_font_t descriptors
FontDescriptor* GWindow::findFont(xcb_font_t fontID) {
    FontDescriptor* fd = (FontDescriptor*)(m_FontList.next);
    int n = 0;  // for safety
    while (fd != (FontDescriptor*)(&m_FontList)) {
        if (fd->font_id == fontID)
            return fd;
        fd = (FontDescriptor*)(fd->next);
        ++n;
        assert(n < SHRT_MAX); // Avoid infinite loop
    }
    return 0;
}

void GWindow::removeFontDescriptor(FontDescriptor* fd) {
    assert(fd != 0);
    fd->prev->link(*(fd->next));
    delete fd;
}

void GWindow::addFontDescriptor(
    xcb_font_t fontID, xcb_query_font_reply_t* fontStructure
) {
    FontDescriptor* fd = new FontDescriptor(fontID, fontStructure);
    // Add in the head of the list
    fd->link(*(m_FontList.next));
    m_FontList.link(*fd);
}

void GWindow::releaseFonts() {
    FontDescriptor* fd = (FontDescriptor*)(m_FontList.next);
    int n = 0;  // for safety
    while (fd != (FontDescriptor*)(&m_FontList)) {
        if (m_Connection != 0){
            xcb_close_font(m_Connection, fd->font_id);
            free(fd->font_info);
        }
        m_FontList.link(*(fd->next));
        delete fd;
        fd = (FontDescriptor*)(m_FontList.next);
        ++n;
        assert(n < SHRT_MAX);   // Avoid infinite loop
    }
}

//
// End of file "graph.cpp"
