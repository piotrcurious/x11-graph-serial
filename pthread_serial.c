



// C linux program implementing serial plotting of serial data in CSV input using X11

// Assume first data field is timestamp and following data fields are float data values

// Support up to 8 data values

// Implement running serial parsing running on separate thread and implement semaphoring for copying of buffer so graph display works asynchronous from data collection

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <pthread.h>

#include <semaphore.h>

#include <unistd.h>

#include <X11/Xlib.h>

#include <X11/Xutil.h>

#define MAX_DATA 8 // maximum number of data values

#define MAX_BUF 1024 // maximum size of input buffer

#define WINDOW_WIDTH 800 // width of the window

#define WINDOW_HEIGHT 600 // height of the window

#define MARGIN 50 // margin around the graph

#define COLOR 0x00FF00 // color of the graph

// structure to store a single data point

typedef struct {

    double timestamp; // time in seconds

    double values[MAX_DATA]; // data values

} DataPoint;

// structure to store a circular buffer of data points

typedef struct {

    DataPoint *data; // array of data points

    int size; // size of the array

    int start; // index of the first element

    int end; // index of the last element

    int count; // number of elements in the buffer

} DataBuffer;

// global variables

DataBuffer buffer; // buffer to store the data points

sem_t mutex; // semaphore to synchronize access to the buffer

int serial_fd; // file descriptor for the serial port

int num_data; // number of data values per line

// function to initialize the buffer with a given size

void init_buffer(int size) {

    buffer.data = (DataPoint *)malloc(size * sizeof(DataPoint)); // allocate memory for the array

    buffer.size = size; // set the size of the array

    buffer.start = 0; // set the start index to 0

    buffer.end = -1; // set the end index to -1

    buffer.count = 0; // set the count to 0

}

// function to free the memory allocated for the buffer

void free_buffer() {

    free(buffer.data); // free the memory for the array

}

// function to check if the buffer is empty

int is_empty() {

    return buffer.count == 0; // return true if count is 0, false otherwise

}

// function to check if the buffer is full

int is_full() {

    return buffer.count == buffer.size; // return true if count is equal to size, false otherwise

}

// function to add a data point to the end of the buffer

void enqueue(DataPoint point) {

    sem_wait(&mutex); // wait for the semaphore to be available

    if (is_full()) { // if the buffer is full, overwrite the oldest element

        buffer.start = (buffer.start + 1) % buffer.size; // increment the start index modulo size

        buffer.count--; // decrement the count

    }

    buffer.end = (buffer.end + 1) % buffer.size; // increment the end index modulo size

    buffer.data[buffer.end] = point; // copy the data point to the end of the array

    buffer.count++; // increment the count

    sem_post(&mutex); // release the semaphore

}

// function to remove a data point from the start of the buffer and return it

DataPoint dequeue() {

    sem_wait(&mutex); // wait for the semaphore to be available

    DataPoint point = buffer.data[buffer.start]; // copy the data point from the start of the array

    if (!is_empty()) { // if the buffer is not empty, update the start index and count 

        buffer.start = (buffer.start + 1) % buffer.size; // increment the start index modulo size 

        buffer.count--; // decrement the count 

    }

    sem_post(&mutex); // release the semaphore 

    return point; // return the data point 

}

// function to parse a line of CSV input and store it as a data point 

void parse_line(char *line) {

    DataPoint point; // create a data point 

    char *token; // create a pointer for tokenizing 

    int i = 0; // create an index for iterating 

    token = strtok(line, ","); // get the first token separated by comma 

    while (token != NULL && i <= num_data) { // while there are more tokens and not exceeding num_data 

        if (i == 0) { // if this is the first token, it is assumed to be timestamp 

            point.timestamp = atof(token); // convert it to double and store it as timestamp 

        } else { // otherwise, it is assumed to be a data value 

            point.values[i-1] = atof(token); // convert it to double and store it as a value 

        }

        i++; // increment i 

        token = strtok(NULL, ","); // get next token separated by comma 

    }

    enqueue(point); // add this data point to end of queue 

}

// function to read from serial port and parse lines of CSV input 

void *read_serial(void *arg) {

    char buf[MAX_BUF]; // create a char array for input buffer 

    int n; // create an int for number of bytes read 

    while (1) { // loop forever 

        n = read(serial_fd, buf, MAX_BUF); // read from serial port up to MAX_BUF bytes 

        if (n > 0) { // if there are bytes read 

            buf[n] = '\0'; // add null terminator at end of buf 

            parse_line(buf); // parse this line as CSV input 

        }

        usleep(10000); // sleep for 10 milliseconds 

    }

}

// function to draw a line on a window using X11 graphics context 

void draw_line(Display *display, Window window, GC gc, int x1, int y1, int x2, int y2) {

    XDrawLine(display, window, gc, x1, y1, x2, y2); 



    // draw a line from (x1, y1) to (x2, y2) 

}

// function to draw a graph on a window using X11 graphics context 

void draw_graph(Display *display, Window window, GC gc) {

    int i, j; // create indices for iterating 

    int x1, y1, x2, y2; // create coordinates for drawing lines 

    double min_x, max_x, min_y, max_y; // create variables for scaling 

    double scale_x, scale_y; // create variables for scaling 

    DataPoint point; // create a data point for storing 

    XClearWindow(display, window); // clear the window 

    // draw the axes 

    draw_line(display, window, gc, MARGIN, MARGIN, MARGIN, WINDOW_HEIGHT - MARGIN); // draw the y-axis 

    draw_line(display, window, gc, MARGIN, WINDOW_HEIGHT - MARGIN, WINDOW_WIDTH - MARGIN, WINDOW_HEIGHT - MARGIN); // draw the x-axis 

    // find the minimum and maximum values of x and y 

    min_x = max_x = buffer.data[buffer.start].timestamp; // initialize min_x and max_x to the first timestamp 

    min_y = max_y = buffer.data[buffer.start].values[0]; // initialize min_y and max_y to the first value 

    for (i = 0; i < buffer.count; i++) { // loop through all the data points in the buffer 

        point = buffer.data[(buffer.start + i) % buffer.size]; // get the data point at index (start + i) modulo size 

        if (point.timestamp < min_x) min_x = point.timestamp; // update min_x if timestamp is smaller 

        if (point.timestamp > max_x) max_x = point.timestamp; // update max_x if timestamp is larger 

        for (j = 0; j < num_data; j++) { // loop through all the values in the data point 

            if (point.values[j] < min_y) min_y = point.values[j]; // update min_y if value is smaller 

            if (point.values[j] > max_y) max_y = point.values[j]; // update max_y if value is larger 

        }

    }

    // calculate the scaling factors for x and y 

    scale_x = (WINDOW_WIDTH - 2 * MARGIN) / (max_x - min_x); // scale_x is the ratio of window width to x range 

    scale_y = (WINDOW_HEIGHT - 2 * MARGIN) / (max_y - min_y); // scale_y is the ratio of window height to y range 

    // draw the graph lines for each data value 

    for (j = 0; j < num_data; j++) { // loop through all the data values 

        for (i = 0; i < buffer.count - 1; i++) { // loop through all the data points except the last one 

            point = buffer.data[(buffer.start + i) % buffer.size]; // get the data point at index (start + i) modulo size 

            x1 = MARGIN + (int)((point.timestamp - min_x) * scale_x); // calculate x1 by scaling and translating timestamp 

            y1 = WINDOW_HEIGHT - MARGIN - (int)((point.values[j] - min_y) * scale_y); // calculate y1 by scaling and translating value 

            point = buffer.data[(buffer.start + i + 1) % buffer.size]; // get the next data point at index (start + i + 1) modulo size

            x2 = MARGIN + (int)((point.timestamp - min_x) * scale_x); // calculate x2 by scaling and translating timestamp

            y2 = WINDOW_HEIGHT - MARGIN - (int)((point.values[j] - min_y) * scale_y); // calculate y2 by scaling and translating value

            draw_line(display, window, gc, x1, y1, x2, y2); // draw a line from (x1, y1) to (x2, y2)

        }

    }

}

// function to display a graph on a window using X11 library

void display_graph() {

    Display *display; // create a pointer for display

    Window window; // create a variable for window

    GC gc; // create a variable for graphics context

    XEvent event; // create a variable for event

    display = XOpenDisplay(NULL); // open a connection to the display server

    if (display == NULL) { // if display is null

        fprintf(stderr, "Cannot open display\n"); // print error message

        exit(1); // exit with error code

    }

    window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0,

                                 WINDOW_WIDTH, WINDOW_HEIGHT,

                                 0,

                                 BlackPixel(display, 0),

                                 WhitePixel(display, 0)); 
 



    // create a simple window with given width, height, border width, border color and background color

    XSelectInput(display, window, ExposureMask); // select exposure events for the window

    XMapWindow(display, window); // map the window to the display

    gc = XCreateGC(display, window, 0, NULL); // create a graphics context for the window

    XSetForeground(display, gc, COLOR); // set the foreground color of the graphics context

    while (1) { // loop forever

        XNextEvent(display, &event); // get the next event from the display

        if (event.type == Expose) { // if the event is an expose event

            draw_graph(display, window, gc); // draw the graph on the window using the graphics context

        }

        usleep(100000); // sleep for 100 milliseconds

    }

    XCloseDisplay(display); // close the connection to the display server

}

// main function

int main(int argc, char *argv[]) {

    pthread_t thread; // create a variable for thread

    int size; // create a variable for buffer size

    if (argc < 3) { // if there are less than 3 arguments

        fprintf(stderr, "Usage: %s serial_port num_data [buffer_size]\n", argv[0]); // print usage message

        exit(1); // exit with error code

    }

    serial_fd = open(argv[1], O_RDONLY | O_NOCTTY | O_NDELAY); // open the serial port in read-only mode with no terminal control and no delay

    if (serial_fd == -1) { // if opening failed

        fprintf(stderr, "Cannot open serial port %s\n", argv[1]); // print error message

        exit(1); // exit with error code

    }

    num_data = atoi(argv[2]); // convert the second argument to integer and store it as num_data

    if (num_data < 1 || num_data > MAX_DATA) { // if num_data is invalid

        fprintf(stderr, "Invalid number of data values: %d\n", num_data); // print error message

        exit(1); // exit with error code

    }

    if (argc > 3) { // if there is a third argument

        size = atoi(argv[3]); // convert it to integer and store it as size

        if (size < 1) { // if size is invalid

            fprintf(stderr, "Invalid buffer size: %d\n", size); // print error message

            exit(1); // exit with error code

        }

    } else { // otherwise

        size = 100; 
   



        // use a default buffer size of 100

    }

    init_buffer(size); // initialize the buffer with the given size

    sem_init(&mutex, 0, 1); // initialize the semaphore with value 1

    pthread_create(&thread, NULL, read_serial, NULL); // create a thread to read from serial port and parse lines of CSV input

    display_graph(); // display a graph on a window using X11 library

    pthread_join(thread, NULL); // wait for the thread to finish

    sem_destroy(&mutex); // destroy the semaphore

    free_buffer(); // free the memory allocated for the buffer

    close(serial_fd); // close the serial port

    return 0; // return with success code

}

