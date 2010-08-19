/*  =========================================================================
    device.c

    Simple device launcher that will start a 0MQ queue, forwarder, or streamer
    device from a JSON configuration file.  The config file format follows a
    standard that is documented in zfl_config[7].  This is an example:

    {
        "verbose": false,
        "type": "queue",
        "iothreads": 1,
        "frontend": {
            "option": {
                "hwm": 1000,
                "swap": 25000000
            },
            "bind": "tcp://eth0:5555",
        },
        "backend": {
            "bind": "tcp://eth0:5556",
        }
    }

    Does not provide detailed error reporting.  To verify your JSON files
    use http://www.jsonlint.com.

    Copyright (c) 1991-2010 iMatix Corporation and contributors

    This file is part of the ZeroMQ Function Library: http://zfl.zeromq.org

    This is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#include <zmq.h>
#include <zfl.h>

//  This example reads from the file device.json.  A realistic application
//  would pass the file name on the command line, at least.
//
int main (int argc, char *argv [])
{
    zfl_config_t
        *config;                //  Configurator object
    char
        *config_name = "device.json";
    FILE
        *config_file;           //  JSON config file stream
    char
        *type;                  //  Requested device type
    int
        device_type,            //  0MQ defined type
        frontend_type,          //  Socket types depending
        backend_type;           //    on the device type
    void
        *frontend,              //  Socket for frontend
        *backend;               //  Socket for backend

    //  Load JSON configuration
    config_file = fopen (config_name, "rb");
    if (!config_file) {
        printf ("E: '%s' doesn't exist or can't be read\n", config_name);
        exit (EXIT_FAILURE);
    }
    config = zfl_config_new (config_file);
    fclose (config_file);
    if (!config) {
        printf ("E: '%s' is not valid JSON\n", config_name);
        exit (EXIT_FAILURE);
    }
    //  Parse device type
    type = zfl_config_type (config);
    if (streq (type, "queue")) {
        frontend_type = ZMQ_XREP;
        backend_type = ZMQ_XREQ;
        device_type = ZMQ_QUEUE;
    }
    else
    if (streq (type, "forwarder")) {
        device_type = ZMQ_FORWARDER;
        frontend_type = ZMQ_SUB;
        backend_type = ZMQ_PUB;
    }
    else
    if (streq (type, "streamer")) {
        device_type = ZMQ_STREAMER;
        frontend_type = ZMQ_PULL;
        backend_type = ZMQ_PUSH;
    }
    else {
        printf ("E: Invalid device type \"%s\"\n", type);
        exit (EXIT_FAILURE);
    }
    //  Create and configure sockets
    frontend = zfl_config_socket (config, "frontend", frontend_type);
    assert (frontend);
    backend = zfl_config_socket (config, "backend", backend_type);
    assert (backend);

    //  Start the device now
    if (zfl_config_verbose (config))
        printf ("I: Starting device...\n");

    zmq_device (device_type, frontend, backend);

    //  Will not actually ever return
    zfl_config_destroy (&config);
    return 0;
}

