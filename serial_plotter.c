



// A simple X11 program for Linux written in C that plots a real-time rolling graph

// and updates it using data from a serial port specified at the command line.

// The serial port data is assumed to be in CSV format, with the first field

// representing the timestamp in milliseconds, and the following fields representing

// data values. The program plots each data field in a different color and supports

// up to 8 data fields.

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <termios.h>

#include <fcntl.h>

#include <X11/Xlib.h>

#include <X11/Xutil.h>

#define MAX_DATA_FIELDS 8 // The maximum number of data fields to plot

#define WINDOW_WIDTH 800 // The width of the window in pixels

#define WINDOW_HEIGHT 600 // The height of the window in pixels

#define MARGIN 50 // The margin around the graph in pixels

#define BUFFER_SIZE 256 // The size of the buffer for reading serial port data

#define MAX_POINTS 1000 // The maximum number of points to store for each data field

// A structure to store a point on the graph

typedef struct {

    int x; // The x coordinate in pixels

    int y; // The y coordinate in pixels

} Point;

// A structure to store a data field

typedef struct {

    char *name; // The name of the data field

    int color; // The color of the data field

    Point points[MAX_POINTS]; // The array of points for the data field

    int count; // The number of points for the data field

} DataField;

// A global variable to store the display pointer

Display *display;

// A global variable to store the window ID

Window window;

// A global variable to store the graphics context

GC gc;

// A global variable to store the serial port file descriptor

int serial_fd;

// A global variable to store the array of data fields

DataField data_fields[MAX_DATA_FIELDS];

// A global variable to store the number of data fields

int num_data_fields;

// A global variable to store the minimum and maximum values of the timestamp and the data fields

int min_timestamp, max_timestamp, min_value, max_value;

// A function to initialize the X11 display and window

void init_x11() {

    // Open a connection to the X server

    display = XOpenDisplay(NULL);

    if (display == NULL) {

        fprintf(stderr, "Error: Cannot open display\n");

        exit(1);

    }

    // Get the default screen number and root window ID

    int screen = DefaultScreen(display);

    Window root = RootWindow(display, screen);

    // Create a simple window with a black background and a white border

    window = XCreateSimpleWindow(display, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 1,

                                 WhitePixel(display, screen), BlackPixel(display, screen));

    // Set the window title and icon name

    XStoreName(display, window, "Real-time Rolling Graph");

    XSetIconName(display, window, "Real-time Rolling Graph");

    // Select input events for the window (exposure and keyboard)

    XSelectInput(display, window, ExposureMask | KeyPressMask);

    // Map the window on the screen

    XMapWindow(display, window);

    // Create a graphics context with a white foreground color and a solid line style

    gc = XCreateGC(display, window, 0, NULL);

    XSetForeground(display, gc, WhitePixel(display, screen));

    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinBevel);

}

// A function to close the X11 display and window

void close_x11() {

    // Free the graphics context

    XFreeGC(display, gc);

    // Destroy the window

    XDestroyWindow(display, window);

    // Close the connection to the X server

    XCloseDisplay(display);

}

// A function to initialize the serial port with the given device name and baud rate

void init_serial(char *device_name, int baud_rate) {

    // Open the serial port device file in read-only mode and without controlling terminal mode

    serial_fd = open(device_name, O_RDONLY | O_NOCTTY);

    if (serial_fd == -1) {

        fprintf(stderr, "Error: Cannot open serial port %s\n", device_name);

        exit(1);

    }

    // Get the current attributes of the serial port

    struct termios options;

    tcgetattr(serial_fd, &options);

    // Set the input and output baud rate to the given value

    cfsetispeed(&options, baud_rate);

    cfsetospeed(&options, baud_rate);

    // Set the character size to 8 bits and disable parity checking and flow control

    options.c_cflag &= ~PARENB;

    options.c_cflag &= ~CSTOPB;

    options.c_cflag &= ~CSIZE;

    options.c_cflag |= CS8;

    options.c_cflag &= ~CRTSCTS;

    // Set the input mode to raw (no processing)

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Set the minimum number of characters for non-canonical read and timeout value (no timeout)

    options.c_cc[VMIN] = 1;

    options.c_cc[VTIME] = 0;

    // Apply the new attributes of the serial port

    tcsetattr(serial_fd, TCSANOW, &options);

}

// A function to close the serial port

void close_serial() {

   close(serial_fd); 

}

// A function to initialize the data fields with random names and colors

void init_data_fields() {

   char *names[] = {"Temperature", "Humidity", "Pressure", "Light", "Sound", "Motion", "Voltage", "Current"}; 

   int colors[] = {0xFF0000 /* red */, 0x00FF00 /* green */, 0x0000FF /* blue */, 0xFFFF00 /* yellow */, 

                   0xFF00FF /* magenta */, 0x00FFFF /* cyan */, 0xFFFFFF /* white */, 0xC0C0C0 /* silver */};

   num_data_fields = 0;

   min_timestamp = max_timestamp = min_value = max_value = -1;

   for (int i = 0; i < MAX_DATA_FIELDS; i++) {

       data_fields[i].name = names[i];

       data_fields[i].color = colors[i];

       data_fields[i].count = 0;

   }

}

// A function to read a line from the serial port into a buffer and return its length (or -1 if error)

int read_line(char *buffer) {

   int index = 0;

   char c;

   while (1) {

       int n = read(serial_fd, &c, 1); // Read one character from the serial port 

       if (n == -1) {

           fprintf(stderr,"Error: Cannot read from serial port\n");

           return -1;

       }

       if (n == 0) {

           return index; // Return if no more characters are available 

       }

       if (c == '\n') {

           buffer[index] = '\0'; // Terminate the buffer with a null character 

           return index; // Return if a newline character is encountered 

       }

       buffer[index++] = c; // Append the character to the buffer 

       if (index == BUFFER_SIZE - 1) {

           buffer[index] = '\0'; // Terminate the buffer with a null character 

           return index; // Return if the buffer is full 

       }

   }

}

// A function to parse a line from the serial port into an array of integers and return its length (or -1 if error)

int parse_line(char *line, int *values) {

   char *token;

   int index = 0;

   token = strtok(line,","); // Get the first token separated by comma 

   while (token != NULL) {

       int value = atoi(token); // Convert the token to an integer 

       values[index++] = value; // Append it to values array 

       token = strtok(NULL,","); // Get next token separated by comma 

       if (index == MAX_DATA_FIELDS + 1) {

           return index; // Return if values array is full 

       }

   }

   return index; // Return number of values parsed 

}

// A function to update data fields with new values from serial port and adjust min/max values accordingly 

void update_data_fields(int *values) {

   int timestamp = values[0]; // Get timestamp from first value 

   num_data_fields = parse_line(line + n + 1 , values + n + 1); 

   for (int i=0;i<num_data_fields;i++) { 

      DataField *data_field=&data_fields[i]; 

      if(data_field->count==MAX_POINTS){ 

         memmove(data_field->points,data_field->points+1,sizeof(Point)*(MAX_POINTS-1)); 

         data_field->count--; 

      } 

      Point point; 

      point.x=timestamp; 

      point.y=values[i+1]; 

      data_field->points[data_field->count++]=point; 

      if(min_timestamp==-1||timestamp<min_timestamp){ 

         min_timestamp=timestamp; 

      } 

      if(max_timestamp==-1||timestamp



>max_timestamp){ 

         max_timestamp=timestamp; 

      } 

      if(min_value==-1||point.y<min_value){ 

         min_value=point.y; 

      } 

      if(max_value==-1||point.y>max_value){ 

         max_value=point.y; 

      } 

   } 

} 

// A function to draw the graph axes and labels 

void draw_axes() { 

   // Draw the x-axis and the y-axis 

   XDrawLine(display,window,gc,MARGIN,MARGIN,MARGIN,WINDOW_HEIGHT-MARGIN); 

   XDrawLine(display,window,gc,MARGIN,WINDOW_HEIGHT-MARGIN,WINDOW_WIDTH-MARGIN,WINDOW_HEIGHT-MARGIN); 

   // Draw the x-axis label (Time) and the y-axis label (Value) 

   XDrawString(display,window,gc,MARGIN+(WINDOW_WIDTH-2*MARGIN)/2-10,WINDOW_HEIGHT-10,"Time",4); 

   XDrawString(display,window,gc,10,MARGIN+(WINDOW_HEIGHT-2*MARGIN)/2-10,"Value",5); 

   // Draw the min and max values of the timestamp and the data fields on the axes 

   char buffer[16]; 

   sprintf(buffer,"%d",min_timestamp); 

   XDrawString(display,window,gc,MARGIN+5,WINDOW_HEIGHT-MARGIN+15,buffer,strlen(buffer)); 

   sprintf(buffer,"%d",max_timestamp); 

   XDrawString(display,window,gc,WINDOW_WIDTH-MARGIN-15,WINDOW_HEIGHT-MARGIN+15,buffer,strlen(buffer)); 

   sprintf(buffer,"%d",min_value); 

   XDrawString(display,window,gc,MARGIN-15,MARGIN+(WINDOW_HEIGHT-2*MARGIN)/2+5,buffer,strlen(buffer)); 

   sprintf(buffer,"%d",max_value); 

   XDrawString(display,window,gc,MARGIN-15,MARGIN+5,buffer,strlen(buffer)); 

} 

// A function to draw the data fields on the graph with different colors and lines

void draw_data_fields() {

    for (int i = 0; i < num_data_fields; i++) {

        DataField *data_field = &data_fields[i];

        // Set the foreground color to the data field color

        XSetForeground(display, gc, data_field->color);

        // Draw the data field name on the top right corner

        XDrawString(display, window, gc,

                    WINDOW_WIDTH - MARGIN - 80 + i * 10,

                    MARGIN + 10 + i * 10,

                    data_field->name,

                    strlen(data_field->name));

        // Draw the data field points on the graph with lines connecting them

        for (int j = 0; j < data_field->count - 1; j++) {

            Point p1 = data_field->points[j];

            Point p2 = data_field->points[j + 1];

            // Map the points to the graph coordinates

            int x1 = MARGIN + (p1.x - min_timestamp) * (WINDOW_WIDTH - 2 * MARGIN) / (max_timestamp - min_timestamp);

            int y1 = WINDOW_HEIGHT - MARGIN - (p1.y - min_value) * (WINDOW_HEIGHT - 2 * MARGIN) / (max_value - min_value);

            int x2 = MARGIN + (p2.x - min_timestamp) * (WINDOW_WIDTH - 2 * MARGIN) / (max_timestamp - min_timestamp);

            int y2 = WINDOW_HEIGHT - MARGIN - (p2.y - min_value) * (WINDOW_HEIGHT - 2 * MARGIN) / (max_value - min_value);

            // Draw a line between the points

            XDrawLine(display, window, gc,

                      x1,

                      y1,

                      x2,

                      y2);

        }

    }

    // Reset the foreground color to white

    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

}

// A function to handle exposure events by redrawing the window

void handle_expose(XExposeEvent *event) {

    // Clear the window with a black background

    XClearWindow(display, window);

    // Draw the graph axes and labels

    draw_axes();

    // Draw the data fields on the graph

    draw_data_fields();

}

// A function to handle keyboard events by exiting the program if q or Q is pressed

void handle_keypress(XKeyEvent *event) {

    char buffer[16];

    int n;

    KeySym keysym;

    // Get the key pressed and its symbol

    n = XLookupString(event, buffer, sizeof(buffer), &keysym , NULL);

    // Exit if q or Q is pressed

    if ((n == 1) && ((buffer[0] == 'q') || (buffer[0] == 'Q'))) {

        exit(0);

    }

}

// The main function of the program

int main(int argc , char **argv) {

    // Check if enough arguments are given

    if (argc != 3) {

        fprintf(stderr,"Usage: %s <serial port device> <baud rate>\n", argv[0]);

        exit(1);

    }

    // Initialize the X11 display and window

    init_x11();

    // Initialize the serial port with given device name and baud rate

    init_serial(argv[1], atoi(argv[2]));

    // Initialize the data fields with random names and colors

    init_data_fields();

    // Create a buffer for reading serial port data

    char buffer[BUFFER_SIZE];

    // Create an array for storing parsed values from serial port data

    int values[MAX_DATA_FIELDS + 1];

    // Enter an infinite loop to handle events and read serial port data

    while (1) {

        // Check if any events are pending in the display queue

        if (XPending(display)) {

            // Get the next event from the display queue

            XEvent event;

            XNextEvent(display , &event);

            // Handle events based on their type

            switch(event.type) {

                case Expose: // Exposure event: redraw window contents

                    handle_expose(&event.xexpose);

                    break;

                case KeyPress: // Keyboard event: exit program if q or Q is pressed

                    handle_keypress(&event.xkey);

                    break;

                default: // Ignore other events

                    break;

            }

        }

        // Read a line from serial port into buffer and get its length

        int n = read_line(buffer);

        // Parse values from buffer into values array and get its length

        int m = parse_line(buffer , values);

        // Check if both lengths are valid (> 0)

        if ((n > 0) && (m > 0)) {

            // Update data fields with new values and adjust min/max values accordingly

            update_data_fields(values);

            // Redraw window contents to reflect new data fields

            handle_expose(NULL);

        }

    }

    // Close serial port device file descriptor.

    close_serial();

    return 0;

}

