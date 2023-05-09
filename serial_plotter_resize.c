

// A simple X11 program for Linux written in C plotting real-time rolling graph and updating it using data from serial port specified at command line.

// Assume serial port data is in CSV format, with first field representing timestamp in milliseconds, and following fields represent data values.

// Plot each data field in different color and support up to 8 data fields containing float values.

// Implement ability to resize the window.

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <termios.h>

#include <fcntl.h>

#include <X11/Xlib.h>

#include <X11/Xutil.h>

#define MAX_DATA_FIELDS 8 // Maximum number of data fields to plot


#define WINDOW_WIDTH 800 // Initial window width

#define WINDOW_HEIGHT 600 // Initial window height

#define MARGIN 50 // Margin around the graph

#define COLOR_BLACK 0 // Color index for black

#define COLOR_RED 1 // Color index for red

#define COLOR_GREEN 2 // Color index for green

#define COLOR_BLUE 3 // Color index for blue

#define COLOR_YELLOW 4 // Color index for yellow

#define COLOR_MAGENTA 5 // Color index for magenta

#define COLOR_CYAN 6 // Color index for cyan

#define COLOR_WHITE 7 // Color index for white

// A structure to store a data point

typedef struct {

    long timestamp; // Timestamp in milliseconds

    float values[MAX_DATA_FIELDS]; // Data values

} DataPoint;

#define MAX_DATA_POINTS 1000 // Maximum number of data points to store 


// A structure to store the graph parameters

typedef struct {

    int width; // Window width

    int height; // Window height

    int num_fields; // Number of data fields to plot

    long min_timestamp; // Minimum timestamp in the data

    long max_timestamp; // Maximum timestamp in the data

    float min_value; // Minimum value in the data

    float max_value; // Maximum value in the data

    int colors[MAX_DATA_FIELDS]; // Colors for each data field

} Graph;

// A global variable to store the display pointer

Display *display;

// A global variable to store the window ID

Window window;

// A global variable to store the graphics context

GC gc;

// A global variable to store the color map

Colormap colormap;

// A global variable to store the color pixels

unsigned long pixels[8];

// A global variable to store the serial port file descriptor

int serial_fd;

// A global variable to store the data buffer

DataPoint buffer[MAX_DATA_POINTS];

// A global variable to store the number of data points in the buffer

int buffer_size;

// A global variable to store the graph parameters

Graph graph;

// a global variable to store keypress event
Bool keypress = False; 


// A function to initialize the X11 display and window

void init_x11(char *title) {

    // Open the display connection

    display = XOpenDisplay(NULL);

    if (display == NULL) {

        fprintf(stderr, "Error: Cannot open display\n");

        exit(1);

    }

    // Get the default screen and root window ID

    int screen = DefaultScreen(display);

    Window root = RootWindow(display, screen);

    // Get the default color map and allocate some colors

    colormap = DefaultColormap(display, screen);

    XColor color;

    XAllocNamedColor(display, colormap, "black", &color, &color);

    pixels[COLOR_BLACK] = color.pixel;

    XAllocNamedColor(display, colormap, "red", &color, &color);

    pixels[COLOR_RED] = color.pixel;

    XAllocNamedColor(display, colormap, "green", &color, &color);

    pixels[COLOR_GREEN] = color.pixel;

    XAllocNamedColor(display, colormap, "blue", &color, &color);

    pixels[COLOR_BLUE] = color.pixel;

    XAllocNamedColor(display, colormap, "yellow", &color, &color);

    pixels[COLOR_YELLOW] = color.pixel;

    XAllocNamedColor(display, colormap, "magenta", &color, &color);

    pixels[COLOR_MAGENTA] = color.pixel;

    XAllocNamedColor(display, colormap, "cyan", &color, &color);

    pixels[COLOR_CYAN] = color.pixel;

    XAllocNamedColor(display, colormap, "white", &color, &color);

    pixels[COLOR_WHITE] = color.pixel;

    // Create the window with some attributes

    window = XCreateSimpleWindow(display, root,

                                 0, 0,

                                 WINDOW_WIDTH, WINDOW_HEIGHT,

                                 1,

                                 pixels[COLOR_BLACK],

                                 pixels[COLOR_WHITE]);

    

    // Set the window title and icon name

    XStoreName(display, window, title);

    XSetIconName(display, window, title);

    // Select some events to handle

    XSelectInput(display, window,

                 ExposureMask | KeyPressMask | StructureNotifyMask);
    
    // Create a graphics context with some attributes
    XGCValues values; // Create a XGCValues structure
    values.foreground = pixels[COLOR_BLACK]; // Set the foreground color to black
    values.background = pixels[COLOR_WHITE]; // Set the background color to white
    // Create a graphics context with some attributes
    gc = XCreateGC(display, window,
                   GCForeground | GCBackground, // Specify which attributes are set
                   &values); // Pass the pointer to the XGCValues structure    
    

    XSetForeground(display, gc,

                   pixels[COLOR_BLACK]);

    

    XSetBackground(display, gc,

                   pixels[COLOR_WHITE]);

    // Map the window on the screen and flush the output buffer

    XMapWindow(display, window);

    XFlush(display);

}



// A function to close the X11 display and window

void close_x11() {

    // Free the graphics context and the color pixels

    XFreeGC(display, gc);

    XFreeColors(display, colormap, pixels, 8, 0);

    // Destroy the window and close the display connection

    XDestroyWindow(display, window);

    XCloseDisplay(display);

}

// A function to initialize the serial port with the given device name and baud rate

void init_serial(char *device, int baud) {

    // Open the serial port device file in read-only mode

    serial_fd = open(device, O_RDONLY | O_NOCTTY);

    if (serial_fd == -1) {

        fprintf(stderr, "Error: Cannot open serial port %s\n", device);

        exit(1);

    }

    // Get the current terminal attributes of the serial port

    struct termios options;

    tcgetattr(serial_fd, &options);

    // Set the input and output baud rate to the given value

    cfsetispeed(&options, baud);

    cfsetospeed(&options, baud);

    // Set the terminal attributes of the serial port

    tcsetattr(serial_fd, TCSANOW, &options);

}

// A function to close the serial port

void close_serial() {

    // Close the serial port device file

    close(serial_fd);

}

// A function to read a line from the serial port and parse it as a data point

// Return 1 if successful, 0 if end of file, -1 if error

int read_data_point(DataPoint *data_point) {

    // Initialize a buffer to store the line

    char line[256];

    int index = 0;

    // Read characters from the serial port until a newline or end of file is encountered

    while (1) {

        // Read one character from the serial port

        char c;

        int n = read(serial_fd, &c, 1);

        // Check for end of file or error

        if (n == 0) {

            return 0;

        }

        if (n == -1) {

            fprintf(stderr, "Error: Cannot read from serial port\n");

            return -1;

        }

        // Check for newline or buffer overflow

        if (c == '\n' || index == 255) {

            break;

        }

        // Append the character to the buffer

        line[index++] = c;

    }

    // Check if the line buffer is empty
     if (index == 0 ) {
         return 0 ; 
     }
    
    // Check if the line buffer has a trailing CR character and remove it 
     if (line [index - 1] == '\r') {
         index--; 
     }
    
    // Terminate the buffer with a null character

    line[index] = '\0';

    // Parse the buffer as a comma-separated list of values

    char *token ;
    token = strtok(line, ",");

    // The first token should be the timestamp in milliseconds

    if (token == NULL) {

        fprintf(stderr, "Error: Invalid data format\n");

        return 0;
    }
    
    data_point->timestamp = atol(token);
   
    // The following tokens should be the data values
        // Get the next token
        token = strtok(NULL, ",");

    int i = 0;

    while (token != NULL && i < MAX_DATA_FIELDS) {

        // Convert the token to a float value and store it in the data point
        data_point->values[i++] = atof(token);
        // Get the next token
        token = strtok(NULL, ",");


    }

    // The number of tokens should match the number of data fields

    if (token != NULL || i != graph.num_fields) {

        fprintf(stderr, "Error: Invalid data format\n");

        return 0;

    }

    // Return success

    return 1;

}

// A function to update the graph parameters based on the data buffer

void update_graph() {

    // Initialize the graph parameters with some default values

    graph.width = WINDOW_WIDTH;

    graph.height = WINDOW_HEIGHT;

    graph.min_timestamp = 0;

    graph.max_timestamp = 1000;

    graph.min_value = 0;

    graph.max_value = 1;

    

    // Assign different colors to each data field

    graph.colors[0] = COLOR_RED;

    graph.colors[1] = COLOR_GREEN;

    graph.colors[2] = COLOR_BLUE;

    graph.colors[3] = COLOR_YELLOW;

    graph.colors[4] = COLOR_MAGENTA;

    graph.colors[5] = COLOR_CYAN;

    graph.colors[6] = COLOR_BLACK;

    graph.colors[7] = COLOR_WHITE;

    // If the buffer is not empty, update the graph parameters based on the data

    if (buffer_size > 0) {

        // Set the minimum and maximum timestamp to the first and last data point in the buffer

        graph.min_timestamp = buffer[0].timestamp;

        graph.max_timestamp = buffer[buffer_size - 1].timestamp;

        // Set the minimum and maximum value to the first data value in the buffer

        graph.min_value = buffer[0].values[0];

        graph.max_value = buffer[0].values[0];

        // Loop through the buffer and find the minimum and maximum value among all data fields

        for (int i = 0; i < buffer_size; i++) {

            for (int j = 0; j < graph.num_fields; j++) {

                if (buffer[i].values[j] < graph.min_value) {

                    graph.min_value = buffer[i].values[j];

                }

                if (buffer[i].values[j] > graph.max_value) {

                    graph.max_value = buffer[i].values[j];

                }

            }

        }

        

        // Add some margin to the minimum and maximum value

        float margin = (graph.max_value - graph.min_value) * 0.1;

        graph.min_value -= margin;

        graph.max_value += margin;

        

        // If the minimum and maximum value are equal, set them to 0 and 1

        if (graph.min_value == graph.max_value) {

            graph.min_value = 0;

            graph.max_value = 1;

        }

        

        // If the minimum and maximum timestamp are equal, set them to 0 and 1000

        if (graph.min_timestamp == graph.max_timestamp) {

            graph.min_timestamp = 0;

            graph.max_timestamp = 1000;

        }

    }

}

// A function to draw the graph on the window

void draw_graph() {

    // Clear the window with white color

    XSetForeground(display, gc, pixels[COLOR_WHITE]);

    

    XFillRectangle(display, window, gc, 0, 0, graph.width, graph.height);

    // Draw the x-axis and y-axis with black color

    XSetForeground(display, gc, pixels[COLOR_BLACK]);

    
    XDrawLine(display, window, gc, MARGIN, MARGIN, MARGIN, graph.height - MARGIN);

    XDrawLine(display, window, gc, MARGIN, graph.height - MARGIN, graph.width - MARGIN, graph.height - MARGIN);

    // Draw the x-axis and y-axis labels with black color

    char label[32];

    sprintf(label, "%ld ms", graph.min_timestamp);

    XDrawString(display, window, gc, MARGIN, graph.height - MARGIN + 20, label, strlen(label));

    sprintf(label, "%ld ms", graph.max_timestamp);

    XDrawString(display, window, gc, graph.width - MARGIN - 40, graph.height - MARGIN + 20, label, strlen(label));

    sprintf(label, "%.2f", graph.min_value);

    XDrawString(display, window, gc, MARGIN - 40, graph.height - MARGIN + 5, label, strlen(label));
    
    sprintf(label, "%.2f", graph.max_value);

    XDrawString(display, window, gc, MARGIN - 40, MARGIN + 5, label, strlen(label));

    // Draw the data points and lines with different colors for each data field

    for (int i = 0; i < graph.num_fields; i++) {

        // Set the foreground color to the corresponding color for the data field

        XSetForeground(display, gc, pixels[graph.colors[i]]);

        // Loop through the buffer and draw the data points and lines

        for (int j = 0; j < buffer_size; j++) {

            // Calculate the x and y coordinates of the data point on the window

            int x = MARGIN + (buffer[j].timestamp - graph.min_timestamp) * (graph.width - 2 * MARGIN) / (graph.max_timestamp - graph.min_timestamp);

            int y = graph.height - MARGIN - (buffer[j].values[i] - graph.min_value) * (graph.height - 2 * MARGIN) / (graph.max_value - graph.min_value);

            // Draw a small circle around the data point

            XFillArc(display, window, gc,

                     x - 2, y - 2,

                     4, 4,

                     0, 360 * 64);

            // If this is not the first data point in the buffer, draw a line from the previous data point to this one

            if (j > 0) {

                // Calculate the x and y coordinates of the previous data point on the window

                int prev_x = MARGIN + (buffer[j-1].timestamp - graph.min_timestamp) * (graph.width - 2 * MARGIN) / (graph.max_timestamp - graph.min_timestamp);

                int prev_y = graph.height - MARGIN - (buffer[j-1].values[i] - graph.min_value) * (graph.height - 2 * MARGIN) / (graph.max_value - graph.min_value);

                // Draw a line from the previous data point to this one

                XDrawLine(display, window, gc,

                          prev_x, prev_y,

                          x, y);

            }

        }

    }

    // Flush the output buffer to display the graph on the window

    XFlush(display);

}

// A function to handle the events from the X11 server

void handle_events() {

    // Initialize an event structure to store the event

    XEvent event;

    // Loop until a key press event is received


        // Wait for an event from the X11 server

        XNextEvent(display, &event);

        // Check the type of the event

        switch (event.type) {

            // If it is an expose event, redraw the graph

            case Expose:

                draw_graph();

                break;

            

            // If it is a key press event, exit the loop

            case KeyPress:
		keypress = True;
                break;

            

            // If it is a configure notify event, update the window size and redraw the graph

            case ConfigureNotify:

                graph.width = event.xconfigure.width;

                graph.height = event.xconfigure.height;

                draw_graph();

                break;

            

            // Ignore other types of events

            default:

                break;

        }

    

}

// The main function of the program

int main(int argc, char **argv) {

    // Check if the command line arguments are valid

    if (argc != 3) {

        fprintf(stderr, "Usage: %s <serial device> <number of data fields>\n", argv[0]);

        exit(1);

    }

    // Get the serial device name and number of data fields from the command line arguments

    char *device = argv[1];

    int num_fields = atoi(argv[2]);

    // Check if the number of data fields is valid

    if (num_fields < 1 || num_fields > MAX_DATA_FIELDS) {

        fprintf(stderr, "Error: Number of data fields must be between 1 and %d\n", MAX_DATA_FIELDS);

        exit(1);

    }



    // Initialize the X11 display and window with a title

    char title[64];

    sprintf(title, "Real-time rolling graph from %s", device);

    init_x11(title);

    // Initialize the serial port with the device name and a baud rate of 9600

    init_serial(device, B9600);

    // Initialize the number of data fields in the graph

    graph.num_fields = num_fields;

    // Initialize the buffer size to zero

    buffer_size = 0;

    // Loop until the user presses a key

    while (1) {

        // Read a data point from the serial port

        DataPoint data_point;

        int result = read_data_point(&data_point);

        // Check the result of reading

        if (result == 1) {

            // If successful, add the data point to the buffer

            buffer[buffer_size++] = data_point;

            // If the buffer is full, roll the data in the buffer

            if (buffer_size >= MAX_DATA_POINTS) {
                memmove(buffer, buffer + sizeof(data_point), (buffer_size - 1) * sizeof(data_point));
                buffer_size--;

            }

            // Update the graph parameters based on the buffer
		printf("Test");
            update_graph();

            // Draw the graph on the window

           draw_graph();
        }

        else if (result == 0) {

            // If no data , check if there are new events

            //break;

        }

        else if (result == -1) {

            // If error, exit the program

            exit(1);

        }

        // Check if there is an event from the X11 server

        if (XPending(display) > 0) {

            // Handle the event and break the loop if it is a key press event

            handle_events();
		if (keypress == True){
            break;}

        }

    }

    // Close the serial port

    close_serial();

    // Close the X11 display and window

    close_x11();

    // Return success

    return 0;

}

