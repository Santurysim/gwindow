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

//bool GWindow::getNextEvent(xcb_generic_event_t* &e) {
//    long eventMask =  
//        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
//        | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_POINTER_MOTION
//        | XCB_EVENT_MASK_STRUCTURE_NOTIFY       // For resize event
//        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
//        | XCB_EVENT_MASK_FOCUS_CHANGE;
//    e = xcb_wait_for_event(m_Connection);
//    return e != NULL//(
//
//        XCheckMaskEvent(m_Connection, eventMask, &e) != 0 ||
//        XCheckTypedEvent(m_Connection, ClientMessage, &e) != 0
//    );
//}

void GWindow::messageLoop(GWindow* dialogWnd /* = 0 */) {
    xcb_generic_event_t *event;

    //+++
    // printf("Message loop...\n");
    //+++

    while (
        m_NumCreatedWindows > 0 &&
        (dialogWnd == 0 || dialogWnd->m_Window != 0)
    ) {
        //... XNextEvent(m_Connection, &event);
//        if (!getNextEvent(event)) {
//            // Sleep a bit
//            timeval dt;
//            dt.tv_sec = 0;
//            dt.tv_usec = 10000;         // 0.01 sec
//            select(1, 0, 0, 0, &dt);    // sleep...
//            continue;
//        }
        event = xcb_wait_for_event(m_Connection);
		if(event){
            // printf("got event: type=%d\n", event.type);
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
    // printf("got event: type=%d\n", event.type);
    GWindow* w; //= findWindow(event->);
    //if (w == 0) {
        /*
        printf(
            "In dispatchEvent: Could not find a window %d, event type=%d\n",
            (int) event.xany.window, (int) event.type
        );
        */
    //    return;
    //}
    if (event->response_type == XCB_EXPOSE) {
		xcb_expose_event_t *exposeEvent = (xcb_expose_event_t*)event;
		w = findWindow(exposeEvent->window);
		if(!w) return;
        // printf("Expose event.\n");
        if (w->m_BeginExposeSeries) {
            w->m_ClipRectangle.x = exposeEvent->x
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
        // printf("KeyPress event.\n");
		xcb_key_press_event_t *keyPressEvent = (xcb_key_press_event_t*)event;
		w = findWindow(keyPressEvent->event);
		if(!w) return;
        w->onKeyPress(keyPressEvent);
    } else if (event->response_type == XCB_BUTTON_PRESS) {
        // printf("ButtonPress event.\n");
		xcb_button_press_event_t *buttonPressEvent = (xcb_button_press_event_t*)event;
		w = findWindow(buttonPressEvent->event);
		if(!w) return;
        w->onButtonPress(buttonPressEvent);
    } else if (event->response_type == XCB_BUTTON_RELEASE) {
		xcb_button_release_event_t *buttonReleaseEvent = (xcb_button_release_event_t*)event;
		w = findWindow(buttonReleaseEvent->event);
		if(!w) return;
        // printf("ButtonRelease event.\n");
        w->onButtonRelease(buttonReleaseEvent);
    } else if (event->response_type == XCB_MOTION_NOTIFY) {
		xcb_motion_notify_event_t *motionNotifyEvent = (xcb_motion_notify_event_t*)event;
		w = findWindow(motionNotifyEvent->event);
		if(!w) return;
        // printf("MotionNotify event.\n");
        w->onMotionNotify(motionNotifyEvent);
    } else if (event->response_type == XCB_CREATE_NOTIFY) {
		xcb_create_notify_event_t *createNotifyEvent = (xcb_create_notify_event_t*)event;
		w = findWindow(createNotifyEvent->window);
		if(!w) return;
        // printf("CreateNotify event: m_Window=%d\n", (int) w->m_Window);
        w->onCreateNotify(createNotifyEvent);
    } else if (event->response_type == XCB_DESTROY_NOTIFY) {
		xcb_create_notify_event_t *createNotifyEvent = (xcb_create_notify_event_t*)event;
		w = findWindow(createNotifyEvent->event);
		if(!w) return;
        /*
        printf(
            "DestroyNotify event: Event window=%d, window=%d\n",
            (int) event.xdestroywindow.event,
            (int) event.xdestroywindow.window
        );
        */
        // printf("Before: m_NumCreatedWindows=%d\n", m_NumCreatedWindows);

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

        // printf("After: m_NumCreatedWindows=%d\n", m_NumCreatedWindows);

    } else if (event->response_type == XCB_FOCUS_IN) {
		xcb_focus_in_event_t *focusInEvent = (xcb_focus_in_event_t*)event;
		w = findWindow(focusInEvent->event);
		if(!w) return;
        // printf("FocusIn event.\n");
        w->onFocusIn(focusInEvent);
    } else if (event->response_type == XCB_FOCUS_OUT) {
		xcb_focus_out_event_t *focusOutEvent = (xcb_focus_out_event_t*)event;
		w = foutdWoutdow(focusOutEvent->event);
		if(!w) return;
        // printf("FocusOut event.\n");
        w->onFocusOut(focusOutEvent);
    // } else if (event->response_type == ResizeRequest) {
    } else if (event->response_type == XCB_CONFIGURE_NOTIFY) {
		xcb_focus_in_event_t *configureEvent = (xcb_focus_in_event_t*)event;
		w = findWindow(configureEvent->window);
		if(!w) return;
        int newWidth = configureEvent->width;
        int newHeight = configureEvent->height;
        // printf("ConfigureNotify: x=%d, y=%d, w=%d, h=%d\n",
        //     configureEvent->x, configureEvent->y,
        //     configureEvent->width, configureEvent->height);
        if (
            newWidth != w->m_IWinRect.width() ||
            newHeight != w->m_IWinRect.height()
        ) {
            // printf("Resize: w=%d, h=%d\n",
            //     configureEvent->width, configureEvent->height);
            w->m_IWinRect.setWidth(newWidth);
            w->m_IWinRect.setHeight(newHeight);
            w->recalculateMap();
            if (w->m_Pixmap != 0) {
                // Offscreen drawing is used
                int depth = m_Screen->root_depth; //DefaultDepth(
                //    m_Connection, DefaultScreen(m_Connection)
                //);
                ::xcb_free_pixmap(m_Connection, w->m_Pixmap);
                w->m_Pixmap = xcb_generate_id(m_Connection);
				::xcb_create_pixmap(
                    m_Connection, depth, w->m_Pixmap,
					w->m_Window, newWidth, newHeight
                );
				// TODO: pixmap might not be ready before onResize
            }
            w->onResize(configureEvent);
            w->redraw();
        }
    } else if (event->response_type == XCB_CLIENT_MESSAGE) { // Window closing, etc.
		xcb_client_message_event_t *clientMessageEvent = (xcb_client_message_event_t*)event;
		w = findWindow(clientMessageEvent->window);
		if(!w) return;
        w->onClientMessage(clientMessageEvent);
    
	free(event);
}

void GWindow::doModal() {
    messageLoop(this);
}

GWindow* GWindow::findWindow(Window w) {
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
    unsigned int wndClass /* = InputOutput */,      // InputOnly, CopyFromParent
    Visual* visual        /* = CopyFromParent */,
    unsigned long attributesValueMask /* = 0 */,    // which attr. are defined
    XSetWindowAttributes* attributes  /* = 0 */     // attributes structure
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

    //+++
    // printf("Creating window: width=%d, height=%d\n",
    //     m_IWinRect.width(), m_IWinRect.height());
    //+++

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
        XColor bg;
        XParseColor(
            m_Connection,
            DefaultColormap(m_Connection, m_Screen),
            m_bgColorName,
            &bg
        );
        XAllocColor(
            m_Connection, DefaultColormap(m_Connection, m_Screen), &bg
        );
        m_bgPixel = bg.pixel;
    } else {
        m_bgPixel = WhitePixel(m_Connection, m_Screen);
    }

    if (m_fgColorName != 0) {
        XColor fg;
        XParseColor(
            m_Connection,
            DefaultColormap(m_Connection, m_Screen),
            m_fgColorName,
            &fg
        );
        XAllocColor(
            m_Connection, DefaultColormap(m_Connection, m_Screen), &fg
        );
        m_fgPixel = fg.pixel;
    } else {
        m_fgPixel = BlackPixel(m_Connection, m_Screen);
    }

    m_BorderWidth = borderWidth;

    /*...
    m_Window = XCreateSimpleWindow(
        m_Connection, 
        DefaultRootWindow(m_Connection), 
        m_WindowPosition.x,
        m_WindowPosition.y,
        m_IWinRect.width(),
        m_IWinRect.height(),
        m_BorderWidth,
        m_fgPixel,
        m_bgPixel
    );
    ...*/

    Window parent;
    if (parentWindow != 0 && parentWindow->m_Window != 0)
        parent = parentWindow->m_Window;
    else
        parent = DefaultRootWindow(m_Connection);
    XSetWindowAttributes windowAttributes;
    XSetWindowAttributes* winAttributes = &windowAttributes;
    if (attributesValueMask != 0 && attributes != 0)
        winAttributes = attributes;
    else
        memset(&windowAttributes, 0, sizeof(windowAttributes));

    m_Window = XCreateWindow(
        m_Connection,
        parent,
        m_WindowPosition.x,
        m_WindowPosition.y,
        m_IWinRect.width(),
        m_IWinRect.height(),
        m_BorderWidth,
        CopyFromParent,
        wndClass,
        visual,
        attributesValueMask,
        winAttributes
    );

    m_WindowCreated = true;
    XSetStandardProperties(
        m_Connection,
        m_Window,
        m_WindowTitle,          // Window name
        m_WindowTitle,          // Icon name
        None,                   // Icon pixmap
        0,                      // argv
        0,                      // argc
        0                       // XSizeHints
    );
    XSelectInput(
        m_Connection,
        m_Window,
        ExposureMask | ButtonPressMask | ButtonReleaseMask
            | KeyPressMask | PointerMotionMask
            | StructureNotifyMask       // For resize event
            | SubstructureNotifyMask
            | FocusChangeMask
    );
    m_GC = XCreateGC(
        m_Connection,
        m_Window,
        0,
        0
    );
    XSetBackground(
        m_Connection,
        m_GC,
        m_bgPixel
    );
    XSetForeground(
        m_Connection,
        m_GC,
        m_fgPixel
    );
    XClearWindow(
        m_Connection,
        m_Window
    );
    XMapRaised(
        m_Connection,
        m_Window
    );

    // To prevent application closing on pressing the window close box
    XSetWMProtocols(
        m_Connection, m_Window,
        &m_WMDeleteWindowAtom, 1
    );

    // printf("In createWindow: m_Window = %d\n", (int) m_Window);
}

void GWindow::setWindowTitle(const char* title) {
    strncpy(m_WindowTitle, title, 127);
    m_WindowTitle[127] = 0;

    if (m_WindowCreated)
        XStoreName(m_Connection, m_Window, m_WindowTitle);
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

    // printf("In destroyWindow: m_Window = %d\n", (int) m_Window);

    if (!m_WindowCreated)
        return;
    m_WindowCreated = false;
    m_NumCreatedWindows--;

    if (m_GC != 0) {
        XFreeGC(
            m_Connection,
            m_GC
        );
        m_GC = 0;
    }
    if (m_Pixmap != 0) {
        XFreePixmap(
            m_Connection,
            m_Pixmap
        );
        m_Pixmap = 0;
    }
    if (m_Window != 0) {
        XDestroyWindow(
            m_Connection,
            m_Window
        );
        m_Window = 0;
    }
}

bool GWindow::initX() {
    //+++
    // printf("Initializing display...\n");
    //+++
	
	xcb_intern_atom_cookie_t WMProtocolsCookie, WMDeleteWindowCookie;
	xcb_intern_atom_reply_t *reply;
	xcb_generic_error_t *atomError;

    m_Connection = xcb_connect(NULL, &m_Screen);
    if (m_Connection == 0) {
        perror("Cannot open display");
        return false;
    }

    // m_Screen  = DefaultScreen(m_Connection);

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
	for(xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(m_Connection)),
			tempScreen = m_Screen; iter.rem; --tempScreen, xcb_screen_next(&iter)){
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

    //+++
    // printf("Closing display...\n");
    //+++

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
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    if (
        abs(p1.x) < SHRT_MAX && abs(p1.y) < SHRT_MAX &&
        abs(p2.x) < SHRT_MAX && abs(p2.y) < SHRT_MAX
    )
    {
        ::XDrawLine(
            m_Connection,
            draw,
            m_GC,
            p1.x, p1.y,
            p2.x, p2.y
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
            ::XDrawLine(
                m_Connection,
                draw,
                m_GC,
                (int)(c1.x + 0.5), (int)(c1.y + 0.5),
                (int)(c2.x + 0.5), (int)(c2.y + 0.5)
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
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    R2Point c1, c2;
    if (m_RWinRect.clip(p1, p2, c1, c2)) {
        I2Point ip1 = map(c1), ip2 = map(c2);

        // printf("Line from (%d, %d) to (%d, %d)\n",
        //     ip1.x, ip1.y, ip2.x, ip2.y);

        ::XDrawLine(
            m_Connection,
            draw,
            m_GC,
            ip1.x, ip1.y,
            ip2.x, ip2.y
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
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    ::XFillRectangle(
        m_Connection,
        draw,
        m_GC,
        r.left(), r.top(), r.width(), r.height()
    );
}

void GWindow::fillRectangle(const R2Rectangle& r, bool offscreen /* = false */) {
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    //+++ printf("leftTop = (%d, %d), rightBottom = (%d, %d)\n",
    //+++     leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);

    ::XFillRectangle(
        m_Connection,
        draw,
        m_GC,
        leftTop.x, leftTop.y,
        rightBottom.x - leftTop.x, rightBottom.y - leftTop.y
    );
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

    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;


    XPoint* pnt = new XPoint[numPoints];
    pnt[0].x = (short) points[0].x;
    pnt[0].y = (short) points[0].y;
    for (int i = 1; i < numPoints; ++i) {
        pnt[i].x = (short) points[i].x;
        pnt[i].y = (short) points[i].y;
    }
    ::XFillPolygon(
        m_Connection,
        draw,
        m_GC,
        pnt,
        numPoints,
        Convex,
        CoordModeOrigin
    );
    delete[] pnt;
}

void GWindow::fillEllipse(const I2Rectangle& r, bool offscreen /* = false */) {
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    ::XFillArc(
        m_Connection,
        draw,
        m_GC,
        r.left(), r.top(), r.width(), r.height(),
        0, 360*64
    );
}

void GWindow::fillEllipse(const R2Rectangle& r, bool offscreen /* = false */) {
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    ::XFillArc(
        m_Connection,
        draw,
        m_GC,
        leftTop.x, leftTop.y,
        rightBottom.x - leftTop.x, rightBottom.y - leftTop.y,
        0, 360*64
    );
}

void GWindow::redraw() {
    if (!m_WindowCreated)
        return;
    //... XClearWindow(m_Connection, m_Window);
    redrawRectangle(
        I2Rectangle(0, 0, m_IWinRect.width(), m_IWinRect.height())
    );
}

void GWindow::redrawRectangle(const I2Rectangle& r) {
    XRectangle rect;
    rect.x = (short) r.left(); rect.y = (short) r.top();
    rect.width = (unsigned short) r.width(); 
    rect.height = (unsigned short) r.height();
    XSetClipRectangles(
        m_Connection, m_GC, 
        0, 0,                   // Clip origin
        &rect, 1, Unsorted
    );

    XEvent e;
    memset(&e, 0, sizeof(e));
    e.type = Expose;
    e.xany.window = m_Window;
    e.xexpose.x = r.left();
    e.xexpose.y = r.top();
    e.xexpose.width = r.width();
    e.xexpose.height = r.height();
    e.xexpose.count = 0;
    XSendEvent(m_Connection, m_Window, 0, ExposureMask, &e);
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

    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    ::XDrawString(
        m_Connection,
        draw,
        m_GC,
        x, y,
        str, 
        l
    );
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
    XColor c;
    XParseColor(
        m_Connection, 
        DefaultColormap(m_Connection, m_Screen), 
        colorName,
        &c
    );
    XAllocColor(
        m_Connection, DefaultColormap(m_Connection, m_Screen), &c
    );
    return c.pixel;
}

void GWindow::setBackground(unsigned long bg) {
    XSetBackground(m_Connection, m_GC, bg);
    m_bgPixel = bg;
}

void GWindow::setBackground(const char* colorName) {
    // printf("Setting bg color: %s\n", colorName);
    unsigned long bgPixel = allocateColor(colorName);
    XSetBackground(m_Connection, m_GC, bgPixel);
    m_bgPixel = bgPixel;
}

void GWindow::setForeground(unsigned long fg) {
    XSetForeground(m_Connection, m_GC, fg);
    m_fgPixel = fg;
}

void GWindow::setForeground(const char* colorName) {
    // printf("Setting fg color: %s\n", colorName);
    unsigned long fgPixel = allocateColor(colorName);
    XSetForeground(m_Connection, m_GC, fgPixel);
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

void GWindow::onExpose(XEvent&) {

}

void GWindow::onResize(XEvent&) {

}

void GWindow::onKeyPress(XEvent&) {

}

void GWindow::onButtonPress(XEvent&) {

}

void GWindow::onButtonRelease(XEvent&) {

}

void GWindow::onMotionNotify(XEvent&) {

}

void GWindow::onCreateNotify(XEvent&) {

}

void GWindow::onDestroyNotify(XEvent&) {
    //... printf("DestroyNotify event: m_Window=%d\n", m_Window);
}

void GWindow::onFocusIn(XEvent&) {

}

void GWindow::onFocusOut(XEvent&) {

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

Font GWindow::loadFont(const char* fontName, XFontStruct **font_struct) {
    //... return XLoadFont(m_Connection, fontName);
    XFontStruct *fStruct = XLoadQueryFont(m_Connection, fontName);
    if (fStruct == NULL)
        return 0;
    Font font = fStruct->fid;
    if (font_struct != 0) {
        *font_struct = fStruct;
    }
    addFontDescriptor(font, fStruct);
    return font;
}

void GWindow::unloadFont(Font fontID) {
    //... XUnloadFont(m_Connection, fontID);
    FontDescriptor* fd = findFont(fontID);
    if (fd == 0) {
        XUnloadFont(m_Connection, fontID);
    } else {
        XFreeFont(m_Connection, fd->font_struct);
        removeFontDescriptor(fd);
    }
}

XFontStruct* GWindow::queryFont(Font fontID) const {
    // Normally, the font must be already loaded by XLoadQueryFont
    // (with GWindow::loadFont).
    FontDescriptor* fd = findFont(fontID);
    if (fd != 0) {
        return fd->font_struct;
    } else {
        // Should not come here!
        return XQueryFont(m_Connection, fontID);
    }
}

void GWindow::setFont(Font fontID) {
    XSetFont(m_Connection, m_GC, fontID);
}

// Message from Window Manager, such as "Close Window"
void GWindow::onClientMessage(XEvent& event) {
    /*
    printf(
        "Client message: message_type = %d, atom name = \"%s\"\n",
        (int) event.xclient.message_type,
        XGetAtomName(m_Connection, event.xclient.message_type)
    );
    */

    if (
        event.xclient.message_type == m_WMProtocolsAtom &&
        (Atom) event.xclient.data.l[0] == m_WMDeleteWindowAtom
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
    // printf("Window is to be closed.\n");
    return true;        // Close the window
}

void GWindow::setLineAttributes(
    unsigned int line_width, int line_style,
    int cap_style, int join_style
) {
    XSetLineAttributes(
        m_Connection, m_GC,
        line_width, line_style, cap_style, join_style
    );
}

void GWindow::setLineWidth(unsigned int line_width) {
    XGCValues values;
    unsigned long valuemask = GCLineWidth;
    /*...
    XGetGCValues(
        m_Connection, m_GC,
        valuemask, &values
    );
    ...*/
    values.line_width = (int) line_width;
    XChangeGC(
        m_Connection, m_GC,
        valuemask, &values
    );
}

void GWindow::drawEllipse(const I2Rectangle& r, bool offscreen /* = false */) {
    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    ::XDrawArc(
        m_Connection,
        draw,
        m_GC,
        r.left(), r.top(), r.width(), r.height(),
        0, 360*64
    );
}

void GWindow::drawEllipse(const R2Rectangle& r, bool offscreen /* = false */) {
    I2Point leftTop = map(R2Point(r.left(), r.top()));
    I2Point rightBottom = map(R2Point(r.right(), r.bottom()));

    Drawable draw = m_Window;
    if (offscreen && m_Pixmap != 0)
        draw = m_Pixmap;

    ::XDrawArc(
        m_Connection,
        draw,
        m_GC,
        leftTop.x, leftTop.y,
        rightBottom.x - leftTop.x, rightBottom.y - leftTop.y,
        0, 360*64
    );
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
    int numDepths = 0;
    int *depths = XListDepths(
        m_Connection,
        DefaultScreen(m_Connection),
        &numDepths
    );
    if (depths != 0) {
        for (int i = 0; i < numDepths; ++i) {
            if (d == depths[i])
                return true;
        }
        XFree(depths);
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
    int depth = DefaultDepth(
        m_Connection, DefaultScreen(m_Connection)
    );
    m_Pixmap = ::XCreatePixmap(
        m_Connection, m_Window,
        m_IWinRect.width(), m_IWinRect.height(),
        depth
    );

    /*...
    printf(
        "Creating offscreen pixmap: width=%d, height=%d, pixmap=%ld\n",
        m_IWinRect.width(), m_IWinRect.height(),
        (long) m_Pixmap
    );
    ...*/

    return (m_Pixmap > 0);
}

void GWindow::swapBuffers() {
    if (m_Pixmap > 0) {
        ::XCopyArea(
            m_Connection, m_Pixmap, m_Window, m_GC,
            0, 0,       // Source
            m_IWinRect.width(), m_IWinRect.height(),
            0, 0        // Destination
        );
    }
}

// Work with the list of font descriptors
FontDescriptor* GWindow::findFont(Font fontID) {
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
    Font fontID, XFontStruct* fontStructure
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
        if (m_Connection != 0)
            XFreeFont(m_Connection, fd->font_struct);
        m_FontList.link(*(fd->next));
        delete fd;
        fd = (FontDescriptor*)(m_FontList.next);
        ++n;
        assert(n < SHRT_MAX);   // Avoid infinite loop
    }
}

//
// End of file "graph.cpp"
