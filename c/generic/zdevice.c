/*  =========================================================================
    zdevice.c

    Device launcher that will start a queue, forwarder, or streamer device
    from a JSON configuration file or automagically from the command line.
    The config file follows ZDCF as at http://rfc.zeromq.org/spec:3.  Here
    is an example:

    {
        "context": {
            "iothreads": 1,
            "verbose": true
        },
        "main" : {
            "type": "zqueue",
            "frontend": {
                "option": {
                    "hwm": 1000,
                    "swap": 25000000
                },
                "bind": "tcp://eth0:5555"
            },
            "backend": {
                "bind": "tcp://eth0:5556"
            }
        }
    }

    Does not provide detailed error reporting.  To verify your JSON files
    use http://www.jsonlint.com.

    From the command-line:

    zdevice zqueue ipc://frontend ipc://backend

    Copyright (c) 1991-2010 iMatix Corporation and contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#include <zmq.h>
#include <zfl.h>


static void
    s_start_configured_device (char *filename);
static void
    s_start_automagic_device (char *type, char *frontend, char *backend);
static void
    s_parse_device_type (char *type, int *device, int *frontend, int *backend);


int
main (int argc, char *argv [])
{
    if (argc == 2)
        s_start_configured_device (argv [1]);
    else
    if (argc == 4)
        s_start_automagic_device (argv [1], argv [2], argv [3]);
    else {
        printf ("\nzdevice/1.0 - start standard 0MQ device\n\n");
        printf ("    Copyright (c) 1991-2010 iMatix Corporation and contributors\n");
        printf ("    Part of the ZeroMQ Function Library: http://zfl.zeromq.org\n");
        printf ("    License: GPL3+\n\n");

        printf ("zdevice CONFIG | DEVICE FRONTEND BACKEND\n\n");
        printf ("CONFIG:\n    ZDCF[1] config file or '-' meaning stdin\n");
        printf ("DEVICE:\n    'zqueue', 'zforwarder', or 'zstreamer'\n");
        printf ("FRONTEND:\n    Endpoint for device frontend socket\n");
        printf ("BACKEND:\n    Endpoint for device backend socket\n\n");

        printf ("[1] ZeroMQ Device Configuration Format - http://rfc.zeromq.org/spec:3\n");
    }
    return 0;
}


//  Starts a device as configured by a full ZDCF config file
//
static void
s_start_configured_device (char *filename)
{
    FILE *file;                 //  JSON config file stream
    if (streq (filename, "-"))
        file = stdin;           //  "-" means read from stdin
    else {
        file = fopen (filename, "rb");
        if (!file) {
            printf ("E: '%s' doesn't exist or can't be read\n", filename);
            exit (EXIT_FAILURE);
        }
    }
    //  Load config data into blob
    zfl_blob_t *blob = zfl_blob_new ();
    assert (blob);
    zfl_blob_load (blob, file);
    fclose (file);

    //  Create a new config from the blob data
    zfl_config_t *config = zfl_config_new (zfl_blob_data (blob));
    assert (config);

    //  Find first device
    char *device = zfl_config_device (config, 0);
    if (!*device) {
        printf ("E: No device specified, please read http://rfc.zeromq.org/spec:3\n");
        exit (EXIT_FAILURE);
    }
    //  Process device type
    char *type = zfl_config_device_type (config, device);

    int device_type;            //  0MQ defined type
    int frontend_type;          //  Socket types depending
    int backend_type;           //    on the device type
    s_parse_device_type (type, &device_type, &frontend_type, &backend_type);

    //  Create and configure sockets
    void *frontend = zfl_config_socket (config, device, "frontend", frontend_type);
    assert (frontend);
    void *backend = zfl_config_socket (config, device, "backend", backend_type);
    assert (backend);

    //  Start the device now
    if (zfl_config_verbose (config))
        printf ("I: Starting device...\n");

    //  Will not actually ever return
    zmq_device (device_type, frontend, backend);

    zfl_config_destroy (&config);
}


//  Parse device type and set device/frontend/backend values for
//  calling zmq_device(3).
//
static void
s_parse_device_type (char *type, int *device, int *frontend, int *backend)
{
    if (streq (type, "zqueue")) {
        *device = ZMQ_QUEUE;
        *frontend = ZMQ_XREP;
        *backend = ZMQ_XREQ;
    }
    else
    if (streq (type, "zforwarder")) {
        *device = ZMQ_FORWARDER;
        *frontend = ZMQ_SUB;
        *backend = ZMQ_PUB;
    }
    else
    if (streq (type, "zstreamer")) {
        *device = ZMQ_STREAMER;
        *frontend = ZMQ_PULL;
        *backend = ZMQ_PUSH;
    }
    else {
        printf ("E: Invalid device type \"%s\"\n", type);
        exit (EXIT_FAILURE);
    }
}


//  Starts an automagical device
//
//  - zqueue acts as broker, binds XREP and XREQ
//  - zforwarder acts as proxy, connects SUB, binds PUB
//  - zstreamer acts as proxy, binds PULL, connects PUSH
//
static void
s_start_automagic_device (char *type, char *frontend, char *backend)
{
    void *context = zmq_init (1);

    int device;                 //  0MQ defined type
    int frontend_type;          //  Socket types depending
    int backend_type;           //    on the device type
    s_parse_device_type (type, &device, &frontend_type, &backend_type);

    //  Create and configure sockets
    void *frontend_socket = zmq_socket (context, frontend_type);
    assert (frontend_socket);
    void *backend_socket = zmq_socket (context, backend_type);
    assert (backend_socket);

    if (device == ZMQ_QUEUE) {
        printf ("I: Binding to %s for client connections\n", frontend);
        if (zmq_bind (frontend_socket, frontend)) {
            printf ("E: cannot bind to '%s': %s\n", frontend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
        printf ("I: Binding to %s for service connections\n", backend);
        if (zmq_bind (backend_socket, backend)) {
            printf ("E: cannot bind to '%s': %s\n", backend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
    }
    else
    if (device == ZMQ_FORWARDER) {
        printf ("I: Connecting to publisher at %s\n", frontend);
        if (zmq_connect (frontend_socket, frontend)) {
            printf ("E: cannot connect to '%s': %s\n", frontend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
        printf ("I: Binding to %s for subscriber connections\n", backend);
        if (zmq_bind (backend_socket, backend)) {
            printf ("E: cannot bind to '%s': %s\n", backend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
    }
    else
    if (device == ZMQ_STREAMER) {
        printf ("I: Binding to %s for upstream nodes\n", frontend);
        if (zmq_bind (frontend_socket, frontend)) {
            printf ("E: cannot bind to '%s': %s\n", frontend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
        printf ("I: Connecting downstream to %s\n", backend);
        if (zmq_connect (backend_socket, backend)) {
            printf ("E: cannot connect to '%s': %s\n", backend, zmq_strerror (errno));
            exit (EXIT_FAILURE);
        }
    }
    zmq_device (device, frontend_socket, backend_socket);
}
