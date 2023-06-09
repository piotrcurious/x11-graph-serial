// A pthread based linux program implementing graphing csv input coming from reading serial port (on separate thread) using Wayland.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <wayland-client.h>
#include <cairo/cairo.h>

#define SERIAL_PORT "/dev/ttyS0" // change this to your serial port
#define SERIAL_BAUD B9600 // change this to your baud rate
#define SERIAL_BUFFER_SIZE 256 // change this to your buffer size
#define GRAPH_WIDTH 800 // change this to your graph width
#define GRAPH_HEIGHT 600 // change this to your graph height
#define GRAPH_MARGIN 50 // change this to your graph margin
#define GRAPH_COLOR "red" // change this to your graph color

// A struct to store the csv data
typedef struct {
    double x; // the x value
    double y; // the y value
} csv_data_t;

// A global variable to store the csv data array
csv_data_t *csv_data = NULL;

// A global variable to store the csv data size
int csv_data_size = 0;

// A global variable to store the serial port file descriptor
int serial_fd = -1;

// A global variable to store the wayland display
struct wl_display *display = NULL;

// A global variable to store the wayland registry
struct wl_registry *registry = NULL;

// A global variable to store the wayland compositor
struct wl_compositor *compositor = NULL;

// A global variable to store the wayland shell
struct wl_shell *shell = NULL;

// A global variable to store the wayland surface
struct wl_surface *surface = NULL;

// A global variable to store the wayland shell surface
struct wl_shell_surface *shell_surface = NULL;

// A global variable to store the wayland buffer
struct wl_buffer *buffer = NULL;

// A global variable to store the cairo surface
cairo_surface_t *cairo_surface = NULL;

// A global variable to store the cairo context
cairo_t *cairo_context = NULL;

// A function to initialize the serial port
int serial_init() {
    // Open the serial port in read/write mode, non-blocking mode and no controlling terminal mode
    serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd == -1) {
        perror("open");
        return -1;
    }

    // Get the current attributes of the serial port
    struct termios options;
    if (tcgetattr(serial_fd, &options) == -1) {
        perror("tcgetattr");
        return -1;
    }

    // Set the input and output baud rate
    if (cfsetispeed(&options, SERIAL_BAUD) == -1) {
        perror("cfsetispeed");
        return -1;
    }
    if (cfsetospeed(&options, SERIAL_BAUD) == -1) {
        perror("cfsetospeed");
        return -1;
    }

    // Set the character size to 8 bits, no parity, one stop bit and no hardware flow control
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CRTSCTS;

    // Set the raw input mode, no echo, no signal characters and no extended functions
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

    // Set the raw output mode, no processing of output characters
    options.c_oflag &= ~OPOST;

    // Set the minimum number of characters for non-canonical read and timeout value
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    // Apply the new attributes of the serial port
    if (tcsetattr(serial_fd, TCSANOW, &options) == -1) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

// A function to read a line from the serial port
int serial_read_line(char *buffer, int size) {
    int index = 0; // the current index of the buffer
    char c; // the current character read from the serial port

    // Loop until the buffer is full or a newline character is read
    while (index < size - 1) {
        // Read one character from the serial port
        if (read(serial_fd, &c, 1) == 1) {
            // If the character is a newline, break the loop
            if (c == '\n') {
                break;
            }
            // Otherwise, store the character in the buffer and increment the index
            buffer[index] = c;
            index++;
        }
    }

    // Terminate the buffer with a null character
    buffer[index] = '\0';

    // Return the number of characters read
    return index;
}

// A function to parse a csv line and store the data in the global array
int csv_parse_line(char *line) {
    // Allocate memory for a new csv data struct
    csv_data_t *data = malloc(sizeof(csv_data_t));
    if (data == NULL) {
        perror("malloc");
        return -1;
    }

    // Scan the line for two double values separated by a comma
    if (sscanf(line, "%lf,%lf", &data->x, &data->y) != 2) {
        fprintf(stderr, "Invalid csv format: %s\n", line);
        free(data);
        return -1;
    }

    // Reallocate memory for the global csv data array to fit the new data
    csv_data = realloc(csv_data, sizeof(csv_data_t) * (csv_data_size + 1));
    if (csv_data == NULL) {
        perror("realloc");
        free(data);
        return -1;
    }

    // Store the new data in the global array and increment the size
    csv_data[csv_data_size] = *data;
    csv_data_size++;

    // Free the temporary data struct
    free(data);

    return 0;
}

// A function to read and parse csv data from the serial port in a separate thread
void *serial_thread(void *arg) {
    // Create a buffer to store the serial line
    char buffer[SERIAL_BUFFER_SIZE];

    // Create a file descriptor set for the select function
    fd_set fds;

    // Create a timeval struct for the select timeout
    struct timeval tv;

    // Loop until the thread is canceled
    while (1) {
        // Clear the file descriptor set
        FD_ZERO(&fds);

        // Add the serial port file descriptor to the set
        FD_SET(serial_fd, &fds);

        // Set the timeout to 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // Call the select function to check if the serial port is ready for reading
        int ret = select(serial_fd + 1, &fds, NULL, NULL, &tv);
        if (ret == -1) {
            perror("select");
            break;
        }
        else if (ret == 0) {
            // Timeout, no data available
            continue;
        }
        else {
            // Data available, read a line from the serial port
            if (serial_read_line(buffer, SERIAL_BUFFER_SIZE) > 0) {
                // Parse the line and store the data in the global array
                if (csv_parse_line(buffer) == -1) {
                    fprintf(stderr, "Failed to parse csv line: %s\n", buffer);
                }
            }
        }
    }

    // Return NULL as the thread exit value
    return NULL;
}

// A function to handle the registry global event
void registry_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    // If the interface is wl_compositor, bind it to the global variable
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    // If the interface is wl_shell, bind it to the global variable
    else if (strcmp(interface, "wl_shell") == 0) {
        shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
}

// A function to handle the registry global remove event
void registry_global_remove(void *data, struct wl_registry *registry, uint32_t id) {
    // Do nothing
}

// A struct to store the registry listener callbacks
struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// A function to handle the shell surface ping event
void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
    // Send a pong reply to the compositor
    wl_shell_surface_pong(shell_surface, serial);
}

// A function to handle the shell surface configure event
void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface,
                             uint32_t edges, int32_t width, int32_t height) {
    // Do nothing
}

// A function to handle the shell surface popup done event
void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface) {
    // Do nothing
}

// A struct to store the shell surface listener callbacks
struct wl_shell_surface_listener shell_surface_listener = {
    .ping = shell_surface_ping,
    .configure = shell_surface_configure,
    .popup_done = shell_surface_popup_done,
};

// A function to create a wayland buffer from a cairo surface
struct wl_buffer *create_buffer_from_cairo_surface(cairo_surface_t *surface) {

    // Get the width, height and stride of the cairo surface
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);

    // Get the data pointer of the cairo surface
    void *data = cairo_image_surface_get_data(surface);

    // Create a wl_shm_pool from the data pointer and the size
    struct wl_shm_pool *pool = wl_shm_create_pool(display, data, height * stride);

    // Create a wl_buffer from the wl_shm_pool with the same width, height and format as the cairo surface
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    // Destroy the wl_shm_pool
    wl_shm_pool_destroy(pool);

    // Return the wl_buffer
    return buffer;
}

// A function to draw a graph on a cairo surface from the csv data
void draw_graph(cairo_surface_t *surface) {
    // Create a cairo context from the surface
    cairo_t *cr = cairo_create(surface);

    // Clear the surface with white color
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    // Set the line width and color for the graph
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // red

    // Find the minimum and maximum x and y values in the csv data
    double min_x = csv_data[0].x;
    double max_x = csv_data[0].x;
    double min_y = csv_data[0].y;
    double max_y = csv_data[0].y;
    for (int i = 1; i < csv_data_size; i++) {
        if (csv_data[i].x < min_x) {
            min_x = csv_data[i].x;
        }
        if (csv_data[i].x > max_x) {
            max_x = csv_data[i].x;
        }
        if (csv_data[i].y < min_y) {
            min_y = csv_data[i].y;
        }
        if (csv_data[i].y > max_y) {
            max_y = csv_data[i].y;
        }

    }

    // Calculate the scale and offset factors for the x and y axes
    double scale_x = (GRAPH_WIDTH - 2 * GRAPH_MARGIN) / (max_x - min_x);
    double offset_x = GRAPH_MARGIN - min_x * scale_x;
    double scale_y = (GRAPH_HEIGHT - 2 * GRAPH_MARGIN) / (max_y - min_y);
    double offset_y = GRAPH_MARGIN - min_y * scale_y;

    // Move to the first point in the csv data
    cairo_move_to(cr, csv_data[0].x * scale_x + offset_x, csv_data[0].y * scale_y + offset_y);

    // Loop through the rest of the csv data and draw lines to each point
    for (int i = 1; i < csv_data_size; i++) {
        cairo_line_to(cr, csv_data[i].x * scale_x + offset_x, csv_data[i].y * scale_y + offset_y);
    }

    // Stroke the graph
    cairo_stroke(cr);

    // Destroy the cairo context
    cairo_destroy(cr);
}

// A function to update the wayland surface with the graph
void update_surface() {
    // Create a cairo surface with the same size and format as the wayland surface
    cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, GRAPH_WIDTH, GRAPH_HEIGHT);

    // Draw the graph on the cairo surface
    draw_graph(cairo_surface);

    // Create a wayland buffer from the cairo surface
    buffer = create_buffer_from_cairo_surface(cairo_surface);

    // Attach the buffer to the wayland surface
    wl_surface_attach(surface, buffer, 0, 0);

    // Commit the wayland surface
    wl_surface_commit(surface);

    // Flush the display
    wl_display_flush(display);
}

// A function to initialize the wayland display and surface
int wayland_init() {

    // Connect to the wayland display
    display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "Failed to connect to the wayland display\n");
        return -1;
    }

    // Get the wayland registry
    registry = wl_display_get_registry(display);
    if (registry == NULL) {
        fprintf(stderr, "Failed to get the wayland registry\n");
        return -1;
    }

    // Add the registry listener callbacks
    wl_registry_add_listener(registry, &registry_listener, NULL);

    // Roundtrip the display to get the global objects
    wl_display_roundtrip(display);

    // Check if the compositor and shell are available
    if (compositor == NULL || shell == NULL) {
        fprintf(stderr, "Missing wayland global objects\n");
        return -1;
    }

    // Create a wayland surface from the compositor
    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create the wayland surface\n");
        return -1;
    }

    // Create a wayland shell surface from the shell and the surface
    shell_surface = wl_shell_get_shell_surface(shell, surface);
    if (shell_surface == NULL) {
        fprintf(stderr, "Failed to create the wayland shell surface\n");
        return -1;
    }

    // Add the shell surface listener callbacks
    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, NULL);

    // Set the shell surface title
    wl_shell_surface_set_title(shell_surface, "Graph");

    // Set the shell surface role as a toplevel window
    wl_shell_surface_set_toplevel(shell_surface);

    // Update the wayland surface with the graph
    update_surface();

    return 0;
}

// A function to clean up the wayland display and surface
void wayland_cleanup() {
    // Destroy the wayland buffer
    if (buffer != NULL) {
        wl_buffer_destroy(buffer);
    }

    // Destroy the cairo surface
    if (cairo_surface != NULL) {
        cairo_surface_destroy(cairo_surface);
    }

    // Destroy the wayland shell surface
    if (shell_surface != NULL) {
        wl_shell_surface_destroy(shell_surface);
    }

    // Destroy the wayland surface
    if (surface != NULL) {
        wl_surface_destroy(surface);
    }

    // Destroy the wayland shell
    if (shell != NULL) {
        wl_shell_destroy(shell);
    }

    // Destroy the wayland compositor
    if (compositor != NULL) {
        wl_compositor_destroy(compositor);
    }

    // Destroy the wayland registry
    if (registry != NULL) {
        wl_registry_destroy(registry);
    }

    // Disconnect from the wayland display
    if (display != NULL) {
        wl_display_disconnect(display);

    }

    // Free the global csv data array
    if (csv_data != NULL) {
        free(csv_data);
    }
}

// The main function
int main() {
    // Initialize the serial port
    if (serial_init() == -1) {
        fprintf(stderr, "Failed to initialize the serial port\n");
        return -1;
    }

    // Initialize the wayland display and surface
    if (wayland_init() == -1) {
        fprintf(stderr, "Failed to initialize the wayland display and surface\n");
        return -1;
    }

    // Create a pthread for reading and parsing csv data from the serial port
    pthread_t thread;
    if (pthread_create(&thread, NULL, serial_thread, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }

    // Loop until the user presses Ctrl-C
    while (1) {
        // Wait for wayland events and dispatch them
        wl_display_dispatch(display);

        // Update the wayland surface with the graph
        update_surface();
    }

    // Cancel and join the pthread
    pthread_cancel(thread);
    pthread_join(thread, NULL);

    // Clean up the wayland display and surface
    wayland_cleanup();

    // Close the serial port
    close(serial_fd);

    // Exit the program with success
    return 0;
}

// End of the main function
