/*
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>

#define LENGTH(X)   (sizeof X / sizeof X[0])


typedef struct {
    KeySym keysym;
    int x;
    int y;
} MoveBinding;

typedef struct {
    KeySym keysym;
    int button;
} ClickBinding;

typedef struct {
    KeySym keysym;
    int speed;
} SpeedBindings;

/* load configuration */
#include "config.h"


Display *dpy;
int screen;
Window root;
pthread_t movethread;

static unsigned int speed = default_speed;

struct {
    int x;
    int y;
    int move_x;
    int move_y;
} mouseinfo;


int getrootptr(int *x, int *y);
void moverelative(int x, int y);
void click(int button, int is_press);
void handle_keypress(XKeyEvent event);
void handle_keyrelease(XKeyEvent event);
void init_x();
void close_x();


int getrootptr(int *x, int *y) {
    int di;
    unsigned int dui;
    Window dummy;
    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

void moverelative(int x, int y) {
    mouseinfo.x += x;
    mouseinfo.y += y;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, mouseinfo.x, mouseinfo.y);
    XFlush(dpy);
}

void click(int button, int is_press) {
    XTestFakeButtonEvent(dpy, button, is_press, 0);
    XFlush(dpy);
}

void init_x() {
    /* initialize support for concurrent threads */
    XInitThreads();

    dpy=XOpenDisplay((char *)0);
    screen=DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);

    /* turn auto key repeat off */
    XAutoRepeatOff(dpy);
}

void close_x() {
    /* turn auto repeat on again */
    XAutoRepeatOn(dpy);
    XCloseDisplay(dpy);
}

void *moveforever(void *val) {
    /* this is executed in a thread */
    while (1) {
        if (mouseinfo.move_x != 0 || mouseinfo.move_y != 0) {
            moverelative(speed * mouseinfo.move_x, speed * mouseinfo.move_y);
        }
        usleep(1000000 / move_rate);
    }
}

void handle_keypress(XKeyEvent event) {
    unsigned int i;
    KeySym keysym;

    keysym = XkbKeycodeToKeysym(dpy, event.keycode, 
                                0, event.state & ShiftMask ? 1 : 0);

    /* move bindings */
    for (i = 0; i < LENGTH(move_bindings); i++) {
        if (move_bindings[i].keysym == keysym) {
            printf("move: %i, %i\n", move_bindings[i].x, move_bindings[i].y);
            mouseinfo.move_x += move_bindings[i].x;
            mouseinfo.move_y += move_bindings[i].y;
        }
    }

    /* click bindings */
    for (i = 0; i < LENGTH(click_bindings); i++) {
        if (click_bindings[i].keysym == keysym) {
            printf("click: %i\n", click_bindings[i].button);
            click(click_bindings[i].button, True);
        }
    }

    /* speed bindings */
    for (i = 0; i < LENGTH(speed_bindings); i++) {
        if (speed_bindings[i].keysym == keysym) {
            speed = speed_bindings[i].speed;
            printf("speed: %i\n", speed);
        }
    }
}

void handle_keyrelease(XKeyEvent event) {
    unsigned int i;
    KeySym keysym;

    keysym = XkbKeycodeToKeysym(dpy, event.keycode, 
                                0, event.state & ShiftMask ? 1 : 0);

    /* move bindings */
    for (i = 0; i < LENGTH(move_bindings); i++) {
        if (move_bindings[i].keysym == keysym) {
            mouseinfo.move_x -= move_bindings[i].x;
            mouseinfo.move_y -= move_bindings[i].y;
        }
    }

    /* click bindings */
    for (i = 0; i < LENGTH(click_bindings); i++) {
        if (click_bindings[i].keysym == keysym) {
            printf("click release: %i\n", click_bindings[i].button);
            click(click_bindings[i].button, False);
        }
    }

    /* speed bindings */
    for (i = 0; i < LENGTH(speed_bindings); i++) {
        if (speed_bindings[i].keysym == keysym) {
            speed = default_speed;
            printf("speed: %i\n", speed);
        }
    }

    /* exit */ 
    for (i = 0; i < LENGTH(exit_keys); i++) {
        if (exit_keys[i] == keysym) {
            close_x();
            exit(0);
        }
    }
}

int main () {
    int rc;
    XEvent event;

    init_x();

    getrootptr(&mouseinfo.x, &mouseinfo.y);
    mouseinfo.move_x = 0;
    mouseinfo.move_y = 0;

    // start the thread for mouse movement
    rc = pthread_create(&movethread, NULL, &moveforever, NULL);
    if( rc != 0 ) {
        printf("Unable to start thread.\n");
        return EXIT_FAILURE;
    }

    while(1) {
        XNextEvent(dpy, &event);

        switch (event.type) {
            case KeyPress:
                getrootptr(&mouseinfo.x, &mouseinfo.y);
                handle_keypress(event.xkey);
                break;

            case KeyRelease:
                getrootptr(&mouseinfo.x, &mouseinfo.y);
                handle_keyrelease(event.xkey);
                break;
        }
    }
}
