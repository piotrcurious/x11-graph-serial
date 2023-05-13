# x11-graph-serial
X11 simple plotting tool
Written by BingAI 

To compile the code, you need to have a C compiler and the X11 development libraries installed on your Linux system. You can use the `gcc` command to compile the code and link it with the X11 library. For example, if you save the code in a file named `graph.c`, you can compile it with this command:

```bash

gcc graph.c -o graph -lX11

```

This will produce an executable file named `graph` that you can run with the serial device name and number of data fields as arguments. For example, if your serial device is `/dev/ttyUSB0` and you have 4 data fields, you can run the program with this command:

```bash

./graph 1 /dev/ttyUSB0 4

```
Usage: ./graph <color theme number> <serial device> <number of data fields>

color theme number 1 means black background. 

for various strategies of binding usb device under static name :
  
https://unix.stackexchange.com/questions/66901/how-to-bind-usb-device-under-a-static-name
  
  
the  explaination of the resize version:
OK, I'll try to explain the code. Here's a summary of what each function does:

- The `init_x11` function initializes the X11 display and window with some attributes, such as the size, color, title and events to handle. It also allocates some colors and creates a graphics context for drawing on the window.

- The `close_x11` function frees the resources used by the X11 display and window, such as the graphics context, the color pixels, the window and the display connection.

- The `init_serial` function initializes the serial port with the given device name and baud rate. It opens the device file in read-only mode and sets the terminal attributes of the serial port.

- The `close_serial` function closes the serial port device file.

- The `read_data_point` function reads a line from the serial port and parses it as a data point. It assumes that the line is in CSV format, with the first field being the timestamp in milliseconds and the following fields being the data values. It returns 1 if successful, 0 if end of file, -1 if error.

- The `update_graph` function updates the graph parameters based on the data buffer. It sets the window size, the number of data fields, the minimum and maximum timestamp and value, and the colors for each data field. It also adds some margin to the minimum and maximum value and handles some edge cases where they are equal.

- The `draw_graph` function draws the graph on the window. It clears the window with white color, draws the x-axis and y-axis with black color and labels them with the minimum and maximum timestamp and value. It then loops through the buffer and draws the data points and lines with different colors for each data field. -It also flushes the output buffer to display the graph on the window.- It does not flush as flushing is done by Xorg on vsync refresh. 

- The `handle_events` function handles the events from the X11 server. It waits for an event and checks its type. If it is an expose event, it redraws the graph. If it is a key press event, it exits the loop. If it is a configure notify event, it updates the window size and redraws the graph. It ignores other types of events.

- The `main` function is the entry point of the program. It checks if the command line arguments are valid and gets the serial device name and number of data fields from them. It then initializes the X11 display and window with a title that includes the device name. It also initializes the serial port with a baud rate of 9600 and sets the number of data fields in the graph. It then loops until either end of file or key press event is received.

In each iteration of the main loop, it reads a data point from the serial port and checks the result. If successful, it adds the data point to the buffer and removes the oldest one if the buffer is full. It then updates the graph parameters based on the buffer and draws the graph on the window. If end of file, it breaks the  loop. If error, it exits the program.

- It also checks if there is an event from the X11 server and handles it. If it is a key press event, it breaks the loop.

- After the loop, it closes the serial port and the X11 display and window and returns success.
