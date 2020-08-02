//
// File "bezier.cpp"
// Draw a cubic bezier curve
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/cursorfont.h>
#include <assert.h>
#include <gwindow.h>

static const int MAX_POINTS = 8;
static const double CATCH_DIST = 0.1;

//--------------------------------------------------
// Definition of our main class "MyWindow"
//
class MyWindow: public GWindow {    // Our main class
    R2Point p[MAX_POINTS];          // Points
    int n;                          // Number of points
    xcb_font_t cursorFont;
    xcb_cursor_t indicateCursor;
    xcb_cursor_t catchCursor;
    bool nodeIndicated;
    bool nodeCatched;
    int catchedNodeIdx;

    // Offscreen buffer
    bool initialUpdate;
public:
    bool offscreenDrawing;

public:
    MyWindow():                     // Constructor
        n(0),
        indicateCursor(0),
        catchCursor(0),
        nodeIndicated(false),
        nodeCatched(false),
        catchedNodeIdx(-1),
        initialUpdate(true),
        offscreenDrawing(false)
    {
        cursorFont = xcb_generate_id(m_Connection);
        xcb_open_font(m_Connection, cursorFont, strlen("cursor"), "cursor");
    }

    ~MyWindow() {
        releaseCursors();
    }

    void drawBezierCurve(           // Draw a Bezier curve
        int numControlPoints,
        const R2Point *q            // Control points
    );

    R2Point bezierCurve(            // Bezier curve
        int numControlPoints,
        const R2Point *q,           // Control points
        double t                    // 0. <= t <= 1.
    );

    void drawInOffscreen();
    void releaseCursors();

    virtual void onExpose(xcb_expose_event_t* event);
    virtual void onKeyPress(xcb_key_press_event_t* event);
    virtual void onButtonPress(xcb_button_press_event_t* event);
    virtual void onButtonRelease(xcb_button_release_event_t* event);
    virtual void onMotionNotify(xcb_motion_notify_event_t* event);
    virtual void onResize(xcb_configure_notify_event_t* event);
    virtual void onDestroyNotify(xcb_destroy_notify_event_t* event);

    int nearestNode(const R2Point& t, double& dist) const;
    void dragNode(const R2Point& t);
};

//----------------------------------------------------------
// Implementation of class "MyWindow"

//
// Process the Expose event: draw in the window
//
void MyWindow::onExpose(xcb_expose_event_t* /* event */) {
    if (initialUpdate) {
        initialUpdate = false;

        // Try to create the offscreen buffer
        if (!offscreenDrawing) {
            offscreenDrawing = createOffscreenBuffer();
        }
        if (offscreenDrawing)
            drawInOffscreen();
    }

    if (offscreenDrawing) {
        swapBuffers();
        return;
    }

    //??? LDraw: ;

    // Erase a window
    setForeground(getBackground());
    fillRectangle(m_RWinRect);

    setLineWidth(1);

    // Draw the coordinate axes
    drawAxes("black", true, "gray");

    // Draw a graph of function
    if (n > 1) {
        setForeground("red");
        setLineWidth(3);
        drawBezierCurve(n, p);
    }

    // Draw a cross on mouse clicks
    setForeground("blue");
    setLineWidth(2);
    R2Vector dx(0.4, 0.);
    R2Vector dy(0., 0.4);
    for (int i = 0; i < n; ++i) {
        drawLine(p[i]-dx, p[i]+dx);
        drawLine(p[i]-dy, p[i]+dy);
    }
}

void MyWindow::drawInOffscreen() {
    /*...
    printf(
        "drawInOffscreen: m_IWinRect=(%d, %d, %d, %d)\n",
        m_IWinRect.left(), m_IWinRect.top(),
        m_IWinRect.width(), m_IWinRect.height()
    );
    ...*/

    // Erase a window

    setForeground(getBackground());
    fillRectangle(m_RWinRect, true);

    setLineWidth(1);

    // Draw the coordinate axes
    drawAxes("black", true, "gray", true);

    // Draw a graph of function
    if (n > 1) {
        setForeground("red");
        setLineWidth(3);
        drawBezierCurve(n, p);
    }

    // Draw a cross on mouse clicks
    setForeground("blue");
    setLineWidth(2);
    R2Vector dx(0.4, 0.);
    R2Vector dy(0., 0.4);
    for (int i = 0; i < n; ++i) {
        drawLine(p[i]-dx, p[i]+dx, true);
        drawLine(p[i]-dy, p[i]+dy, true);
    }
}

void MyWindow::drawBezierCurve(
    int numPoints,
    const R2Point *q
) {
    if (numPoints <= 1)
        return;
    double t = 0.;
    double dt = 0.02;
    moveTo(q[0]);
    t += dt;
    while (t < 1.) {
        drawLineTo(bezierCurve(numPoints, q, t), offscreenDrawing);
        t += dt;
    }
    drawLineTo(q[numPoints - 1], offscreenDrawing);
}

R2Point MyWindow::bezierCurve(
    int numPoints,
    const R2Point *q,       // Control points
    double t                // 0. <= t <= 1.
) {
    assert(numPoints > 0);
    if (numPoints <= 0.)
        return R2Point(0., 0.);
    if (numPoints == 1)
        return q[0];
    if (numPoints == 2)
        return q[0] * (1. - t) + q[1] * t;

    R2Point p0 = bezierCurve(numPoints-1, q, t);
    R2Point p1 = bezierCurve(numPoints-1, q+1, t);
    return p0 * (1. - t) + p1 * t;
}

//
// Process the KeyPress event:
// if "q" is pressed, then close the window
//
void MyWindow::onKeyPress(xcb_key_press_event_t* event) {
    // TODO
    xcb_keysym_t key = xcb_key_symbols_get_keysym(m_KeySymbols, event->detail, 0);
    //char keyName[256];
    //int nameLen = XLookupString(&(event.xkey), keyName, 255, &key, 0);
    printf("KeyPress: keycode=0x%x, state=0x%x, KeySym=0x%x\n",
        event->detail, event->state, (int) key);
    if(key == 'q')
        destroyWindow();
    // if (nameLen > 0) {
    //     keyName[nameLen] = 0;
    //     printf("\"%s\" button pressed.\n", keyName);
    //     if (keyName[0] == 'q') { // quit => close window
    //         destroyWindow();
    //     }
    // }
}

void MyWindow::onButtonPress(xcb_button_press_event_t* event) {
    int x = event->event_x;
    int y = event->event_y;
    R2Point t = invMap(I2Point(x, y));
    int mouseButton = event->detail;

    printf("Mouse click: x=%d, y=%d, button=%d\n", x, y, mouseButton);

    if (mouseButton != XCB_BUTTON_INDEX_1) {
        n = 0;
        if (!offscreenDrawing) {
            redraw();
        } else {
            drawInOffscreen();
            swapBuffers();
        }
    } else {
        double dist;
        int node;
        node = nearestNode(t, dist);
        if (node >= 0 && dist <= CATCH_DIST) {
            printf("Node is catched.\n");
            nodeCatched = true;
            nodeIndicated = false;
            catchedNodeIdx = node;
            if (catchCursor == 0) {
                catchCursor = xcb_generate_id(m_Connection);
                xcb_create_glyph_cursor(m_Connection, catchCursor, cursorFont,
                cursorFont, XC_cross, XC_cross + 1,
                0, 0, 0, 0, 0, 0);
            }
            xcb_change_window_attributes(m_Connection, m_Window, XCB_CW_CURSOR, &catchCursor);
        }
    }
}

void MyWindow::onButtonRelease(xcb_button_release_event_t* event) {
    int x = event->event_x;
    int y = event->event_y;
    R2Point t = invMap(I2Point(x, y));
    int mouseButton = event->detail;

    printf("Mouse button release: x=%d, y=%d, button=%d\n", x, y, mouseButton);

    if (mouseButton != XCB_BUTTON_INDEX_1)
        return;

    if (nodeCatched) {
        printf("Node is release.\n");
        nodeCatched = false;
        XUndefineCursor(m_Display, m_Window);
        p[catchedNodeIdx] = t;
        catchedNodeIdx = (-1);
    } else {
        if (n >= MAX_POINTS)
            n = 0;
        p[n] = t;
        ++n;
    }
    if (!offscreenDrawing) {
        redraw();
    } else {
        drawInOffscreen();
        swapBuffers();
    }
}

// Process mouse moving
void MyWindow::onMotionNotify(xcb_motion_notify_event_t* event) {
    int x = event->event_x;
    int y = event->event_y;
    R2Point t = invMap(I2Point(x, y));

    double dist;
    int node;
    if (nodeIndicated) {
        node = nearestNode(t, dist);
        if (node < 0 || dist > CATCH_DIST) {
            printf("Stopped node indication.\n");
            nodeIndicated = false;
            XUndefineCursor(m_Display, m_Window);
        }
    } else if (nodeCatched) {
        dragNode(t);
    } else {
        node = nearestNode(t, dist);
        if (node >= 0 && dist <= CATCH_DIST) {
            printf("Started node indication.\n");
            nodeIndicated = true;
            catchedNodeIdx = node;
            if (indicateCursor == 0) {
                indicateCursor = XCreateFontCursor(
                    m_Display, XC_hand1
                );
            }
            XDefineCursor(m_Display, m_Window, indicateCursor);
        }
    }
}

int MyWindow::nearestNode(const R2Point& t, double& dist) const {
    int nodeIdx = (-1);
    double nodeDist = 1e+30;    // Infinity
    for (int i = 0; i < n; ++i) {
        double d = p[i].distance(t);
        if (d < nodeDist) {
            nodeDist = d;
            nodeIdx = i;
        }
    }
    if (nodeIdx >= 0) {
        dist = nodeDist;
        return nodeIdx;
    }
    return (-1);
}

void MyWindow::dragNode(const R2Point& t) {
    if (!nodeCatched || catchedNodeIdx < 0)
        return;
    p[catchedNodeIdx] = t;
    if (!offscreenDrawing) {
        redraw();
    } else {
        drawInOffscreen();
        swapBuffers();
    }
}

void MyWindow::onResize(xcb_configure_notify_event_t* /* event */) {
    if (offscreenDrawing) {
        drawInOffscreen();
        swapBuffers();
    }
}

void MyWindow::onDestroyNotify(xcb_destroy_notify_event_t* /* event */) {
    releaseCursors();
}

void MyWindow::releaseCursors() {
    if (m_Connection != 0) {
        if (indicateCursor != 0) {
            XFreeCursor(m_Display, indicateCursor);
            indicateCursor = 0;
        }
        if (catchCursor != 0) {
            XFreeCursor(m_Display, catchCursor);
            catchCursor = 0;
        }
    }
}

//
// End of class MyWindow implementation
//----------------------------------------------------------

/////////////////////////////////////////////////////////////
// Main: initialize X, create an instance of MyWindow class,
//       and start the message loop
int main() {
    // Initialize X stuff
    if (!GWindow::initX()) {
        printf("Could not connect to X-server.\n");
        exit(1);
    }

    MyWindow w;
    double aspect = (double) GWindow::screenMaxY() /
        (double) GWindow::screenMaxX();
    double width = 30.;
    double height = aspect * width;
    w.createWindow(
        I2Rectangle(                    // Window frame rectangle:
            I2Point(10, 10),            //     left-top corner
            GWindow::screenMaxX()/2,    //     width
            GWindow::screenMaxY()/2     //     height
        ),
        R2Rectangle(                    // Coordinate rectangle:
            R2Point(-width/2., -height/2.), // bottom-right corner
            width, height                   // width, height
        ),
        "Bezier Curve"                  // Window title
    );
    w.setBackground("lightGray");

    if (w.createOffscreenBuffer()) {
        w.offscreenDrawing = true;
        w.drawInOffscreen();
    }

    GWindow::messageLoop();

    GWindow::closeX();
    return 0;
}
