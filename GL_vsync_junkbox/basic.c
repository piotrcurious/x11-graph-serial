#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main() {
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: Unable to open X display\n");
        return 1;
    }

    // Create a GLX context
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo* vi = glXChooseVisual(display, screen, att);
    GLXContext context = glXCreateContext(display, vi, NULL, GL_TRUE);

    // Create a window
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask;
    Window window = XCreateWindow(display, root, 0, 0, 800, 600, 0,
                                  vi->depth, InputOutput, vi->visual,
                                  CWColormap | CWEventMask, &swa);

    // Make the GLX context current
    glXMakeCurrent(display, window, context);

    // Enable VSync (swap interval)
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(display, window, 1); // Enable VSync
    }

    // Main loop
    while (1) {
        // Render your OpenGL scene here
        // ...

        // Swap buffers
        glXSwapBuffers(display, window);
    }

    // Clean up
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
