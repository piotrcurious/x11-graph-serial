#/bin/bash
gcc -Os -static serial_plotter_resize_event.c -o event_serial_plotter_static -lX11 -lev -lxcb -lc -lXau -lXdmcp 
