

// A simple X11 program for Linux written in C plotting real-time rolling graph and updating it using data from serial port specified at command line.
// Assume serial port data is in CSV format, with first field representing timestamp in milliseconds, and following fields represent data values.
// Plot each data field in different color and support up to 8 data fields containing float values.
// Implement ability to resize the window.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <ev.h>
//#include <readline.h>

#define BAUD_RATE B115200

#define MAX_DATA_FIELDS 8 // Maximum number of data fields to plot
#define WINDOW_WIDTH 800 // Initial window width
#define WINDOW_HEIGHT 600 // Initial window height
//#define DATA_POINT_CIRCLE // wheter to draw a circle on each data point position (slow)
#define MARGIN 20 // Margin around the graph
#define INTERNAL_GRAPH_MARGIN 0.001 // Margin for min/max values

#define COLOR_BLACK 0 // Color index for black
#define COLOR_RED 1 // Color index for red
#define COLOR_GREEN 2 // Color index for green
#define COLOR_BLUE 3 // Color index for blue
#define COLOR_YELLOW 4 // Color index for yellow
#define COLOR_MAGENTA 5 // Color index for magenta
#define COLOR_CYAN 6 // Color index for cyan
#define COLOR_GRAY 7 // Color index for white
#define COLOR_WHITE 8 // Color index for white

// A structure to store a data point
typedef struct {
    uint32_t timestamp; // Timestamp in milliseconds
    float values[MAX_DATA_FIELDS]; // Data values
} DataPoint;

#define MAX_DATA_POINTS 2048 // Maximum number of data points to store 
#define DISCARD_DATA_POINTS 3 // amount of data points to discard to synchronize with source
#define LINE_SIZE 256 // max line size (line buffer)

// A structure to store the graph parameters
typedef struct {
    uint16_t width; // Window width
    uint16_t height; // Window height
    uint8_t num_fields; // Number of data fields to plot
    uint32_t min_timestamp; // Minimum timestamp in the data
    uint32_t max_timestamp; // Maximum timestamp in the data
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
unsigned long pixels[9]; // 9 because 9 colors in the palette. 
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
Bool new_serial_data = False; 
// a global variable to indicate new serial data arrived
uint8_t color_theme = 0; 
// a global variable to store color theme

DataPoint data_point;
// a global structure for data point

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
    XAllocNamedColor(display, colormap, "Gray41", &color, &color);
    pixels[COLOR_GRAY] = color.pixel;
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

    // Assign different colors to each data field
    graph.colors[0] = COLOR_RED;
    graph.colors[1] = COLOR_GREEN;
    graph.colors[2] = COLOR_BLUE;
    graph.colors[3] = COLOR_YELLOW;
    graph.colors[4] = COLOR_MAGENTA;
    graph.colors[5] = COLOR_CYAN;
    graph.colors[6] = COLOR_BLACK;
    graph.colors[7] = COLOR_GRAY;

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
//    serial_fd = open(device, O_RDONLY | O_NOCTTY);
    serial_fd = open(device, O_RDONLY | O_NOCTTY | O_NDELAY);
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

    options.c_cflag |= (CLOCAL | CREAD); // enable local mode and receiver
    options.c_cflag &= ~PARENB; // disable parity
    options.c_cflag &= ~CSTOPB; // disable two stop bits
    options.c_cflag &= ~CSIZE; // mask character size bits
    options.c_cflag |= CS8; // set 8 data bits
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // set raw input mode
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // disable software flow control

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
//	usleep(10000); // wait for buffer
    while (1) {
        // Read one character from the serial port
        char c;
        int n = read(serial_fd, &c, 1);

        if (n == 0) { //if no new data
//            return 0;
	usleep (50); // wait a little for the buffer to fill up. 
	// TODO : implement timeout
        }

        if (n == -1) {
            fprintf(stderr, "Error: Cannot read from serial port\n");
//            return -1;
        }
        // Check for newline or buffer overflow
        if (c == '\n' || index == 255) {
            break;
        }
        // Append the character to the buffer
	if (n == 1 ) { // if there is data
         line[index++] = c;
	}
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
//    graph.width = WINDOW_WIDTH;
//    graph.height = WINDOW_HEIGHT;
    graph.min_timestamp = 0;
    graph.max_timestamp = 1000;
    graph.min_value = 0;
    graph.max_value = 1;

//    // Assign different colors to each data field
//    graph.colors[0] = COLOR_RED;
//    graph.colors[1] = COLOR_GREEN;
//    graph.colors[2] = COLOR_BLUE;
//    graph.colors[3] = COLOR_YELLOW;
//    graph.colors[4] = COLOR_MAGENTA;
//    graph.colors[5] = COLOR_CYAN;
//    graph.colors[6] = COLOR_BLACK;
//    graph.colors[7] = COLOR_GRAY;
//  moved to x11_init

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
        float margin = (graph.max_value - graph.min_value) * INTERNAL_GRAPH_MARGIN;
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
	switch (color_theme) {

	case 1 :   // dark color theme
    // Clear the window with white color
    XSetForeground(display, gc, pixels[COLOR_BLACK]);
    XFillRectangle(display, window, gc, 0, 0, graph.width, graph.height);
    // Draw the x-axis and y-axis with black color
    XSetForeground(display, gc, pixels[COLOR_WHITE]);
	break;

	default : 
    // Clear the window with white color
    XSetForeground(display, gc, pixels[COLOR_WHITE]);
    XFillRectangle(display, window, gc, 0, 0, graph.width, graph.height);
    // Draw the x-axis and y-axis with black color
    XSetForeground(display, gc, pixels[COLOR_BLACK]);
	break;
        }

//    XDrawLine(display, window, gc, MARGIN, MARGIN, MARGIN, graph.height - MARGIN);
//    XDrawLine(display, window, gc, MARGIN, graph.height - MARGIN, graph.width - MARGIN, graph.height - MARGIN);
    // Draw the x-axis and y-axis labels with black color
    char label[32];
    sprintf(label, "%ld ms", graph.min_timestamp);
    XDrawString(display, window, gc, MARGIN, graph.height - MARGIN + MARGIN/2, label, strlen(label));
    sprintf(label, "%ld ms", graph.max_timestamp);
    XDrawString(display, window, gc, graph.width - MARGIN - 40, graph.height - MARGIN + MARGIN/2, label, strlen(label));
    sprintf(label, "%.2f", graph.min_value);
    XDrawString(display, window, gc, MARGIN - MARGIN, graph.height - MARGIN + 0, label, strlen(label));
    sprintf(label, "%.2f", graph.max_value);
    XDrawString(display, window, gc, MARGIN - MARGIN, MARGIN + 0, label, strlen(label));

    float x_factor = ( (float) graph.width / (graph.max_timestamp - graph.min_timestamp) ); // calculate once to optimize loops
    float y_factor = (graph.height - 1 * MARGIN) / (graph.max_value - graph.min_value);
    // Draw the data points and lines with different colors for each data field
    for (int i = 0; i < graph.num_fields; i++) {
        // Set the foreground color to the corresponding color for the data field
        XSetForeground(display, gc, pixels[graph.colors[i]]);
        // Loop through the buffer and draw the data points and lines
        for (int j = 0; j < buffer_size; j++) {
            // Calculate the x and y coordinates of the data point on the window
//            unsigned int x = MARGIN + (buffer[j].timestamp - graph.min_timestamp) * (graph.width - 1 * MARGIN) / (graph.max_timestamp - graph.min_timestamp);
//            unsigned int y = graph.height - MARGIN - (buffer[j].values[i] - graph.min_value) * (graph.height - 1 * MARGIN) / (graph.max_value - graph.min_value);
//            unsigned int x = ((buffer[j].timestamp - graph.min_timestamp) * ( (float) graph.width / (graph.max_timestamp - graph.min_timestamp) ));
            uint16_t x = ( (buffer[j].timestamp - graph.min_timestamp) * x_factor);
//            unsigned int y = graph.height - MARGIN - (buffer[j].values[i] - graph.min_value) * (graph.height - 1 * MARGIN) / (graph.max_value - graph.min_value);
            uint16_t y = graph.height - MARGIN - (buffer[j].values[i] - graph.min_value) * y_factor;
            // Draw a small circle around the data point
#ifdef DATA_POINT_CIRCLE
            XFillArc(display, window, gc,
                     x - 2, y - 2,
                     4, 4,
                     0, 360 * 64);
#endif // DATA_POINT_CIRCLE
            // If this is not the first data point in the buffer, draw a line from the previous data point to this one
            if (j > 0) {
                // Calculate the x and y coordinates of the previous data point on the window
//                int prev_x = MARGIN + (buffer[j-1].timestamp - graph.min_timestamp) * (graph.width - 1 * MARGIN) / (graph.max_timestamp - graph.min_timestamp);
//                int prev_y = graph.height - MARGIN - (buffer[j-1].values[i] - graph.min_value) * (graph.height - 1 * MARGIN) / (graph.max_value - graph.min_value);
//                uint16_t prev_x = (buffer[j-1].timestamp - graph.min_timestamp) * (graph.width) / (graph.max_timestamp - graph.min_timestamp);
//                uint16_t prev_y = graph.height - MARGIN - (buffer[j-1].values[i] - graph.min_value) * (graph.height - 1 * MARGIN) / (graph.max_value - graph.min_value);
                uint16_t prev_x = (buffer[j-1].timestamp - graph.min_timestamp) * x_factor;
                uint16_t prev_y = graph.height - MARGIN - (buffer[j-1].values[i] - graph.min_value) * y_factor;
                // Draw a line from the previous data point to this one
                XDrawLine(display, window, gc,
                          prev_x, prev_y,
                          x, y);
            }
        }
    }

    // Flush the output buffer to display the graph on the window
//    XFlush(display);
}

void handle_keypress(XKeyEvent *event) {
    char buffer[16];
    int n;
    KeySym keysym;
    // Get the key pressed and its symbol
    n = XLookupString(event, buffer, sizeof(buffer), &keysym , NULL);
    // Exit if q or Q is pressed
    if ((n == 1) && ((buffer[0] == 'q') || (buffer[0] == 'Q'))) {
        keypress = True;
    }
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
                update_graph(); // just in case we were sleeping and data came in asynchronously
                draw_graph();
                break;

            // If it is a key press event, exit the loop
            case KeyPress:
		handle_keypress(&event.xkey);
                break;

            // If it is a configure notify event, update the window size and redraw the graph
            case ConfigureNotify:
                graph.width = event.xconfigure.width;
                graph.height = event.xconfigure.height;
                update_graph();  // in case data got updated asynchronously
                draw_graph();
                break;

            // Ignore other types of events
            default:
                break;
        }
}

// libev event loop
struct ev_loop *loop;
// libev io watcher
ev_io serial_watcher;
// callback function for serial port data available event
void serial_cb(EV_P_ ev_io *w, int revents)
{
  //  printf("Data available\n");
    // read data up to newline from serial port
//    char line[LINE_SIZE];
//    int n = readline(serial_fd, line, LINE_SIZE);
//    new_serial_data = True; 
        int result = read_data_point(&data_point);
    if (result == -1)
    {
        perror("error reading data");
        exit(1);
    }
    else if (result == 0)
    {
//        printf("No more data\n");
        return ;
    }
    else
    {
//        printf("Read %d bytes: \n", result);
    new_serial_data = True; 
    }

    new_serial_data = True ;// set flag indicating there is new serial data avail
    // Return success
    return ;
//    float data_point = strtof(line, NULL);
//    printf("Data point: %f\n", data_point);
}

// The main function of the program
int main(int argc, char **argv) {

    // Check if the command line arguments are valid
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <color theme number> <serial device> <number of data fields>\n", argv[0]);
        exit(1);
    }

    // Get the serial device name and number of data fields from the command line arguments
    color_theme = atoi(argv[1]); // TODO : implement better color theme handling. now it is simple case: hack
    char *device = argv[2];
    int num_fields = atoi(argv[3]);
    // Check if the number of data fields is valid
    if (num_fields < 1 || num_fields > MAX_DATA_FIELDS) {
        fprintf(stderr, "Error: Number of data fields must be between 1 and %d\n", MAX_DATA_FIELDS);
        exit(1);
    }

    // Initialize the X11 display and window with a title
    char title[64];
    sprintf(title, "%s q to quit. ", device);
    init_x11(title);

    // Initialize the number of data fields in the graph
    graph.num_fields = num_fields;
    // Initialize the buffer size to zero
    buffer_size = 0;

    // Initialize the serial port with the device name and a baud rate 
    init_serial(device, BAUD_RATE);

    // set file descriptor as blocking
    fcntl(serial_fd, F_SETFL, 0);
    // create default event loop
    loop = ev_default_loop(0);
    // initialize io watcher for serial port file descriptor
    ev_io_init(&serial_watcher, serial_cb, serial_fd, EV_READ);
    // start io watcher
    ev_io_start(loop, &serial_watcher);
    // start event loop
    // ev_run(loop, 0);

        printf("discarding first data points\n");

        int i=0;
	while (i < DISCARD_DATA_POINTS) {
    // int result = read_data_point(&data_point);
	while(!new_serial_data){
    // printf("Synchronized\n");
	ev_run(loop,EVRUN_NOWAIT);
		usleep(1000);
         }
	new_serial_data = False; 
	usleep(5000); 
	 i++;
      }
    // printf("No more data2\n");

    // Loop until the user presses a key
    while (1) { // lets 
        // Read a data point from the serial port
//        int result = read_data_point(&data_point);
        // Check the result of reading

        if (new_serial_data == True) {
            // If successful, add the data point to the buffer
            buffer[buffer_size] = data_point;
		buffer_size++;
		new_serial_data = False ; // reset new serial data flag
            // If the buffer is full, roll the data in the buffer
            if (buffer_size >= MAX_DATA_POINTS-2) {
            // memcpy(buffer, buffer + sizeof(DataPoint), 8 * sizeof(DataPoint));
            // for some reason memcpy and memmove does not work. perhaps structs in buffers are padded?
		for (int i=0; i<buffer_size; i++) {
    			buffer[i] = buffer[i+1]; 
		        }
                buffer_size--;
            }
            // Update the graph parameters based on the buffer
            update_graph();
            // Draw the graph on the window
	// TODO: make it being called periodically on Vsync instead all the time
            draw_graph();
        } //if (new_serial_data) {

        else if (!new_serial_data) {
            // If no data , check if there are new events
	usleep(1000); // sleep a little.... 
	ev_run(loop,EVRUN_NOWAIT); // poll for new serial data
        }

 //       else if (result == -1) {
            // If error, exit the program
 //           exit(1);
 //       }
//	usleep(5000); // sleep a little.... 
//            update_graph();
//            draw_graph();

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
