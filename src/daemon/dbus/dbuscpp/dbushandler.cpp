/* This file is part of Strigi Desktop Search
 *
 * Copyright (C) 2006 Jos van den Oever <jos@vandenoever.info>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "dbushandler.h"
#include "dbusobjectcallhandler.h"
#include "dbusobjectinterface.h"

#include <iostream>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;

DBusHandler::DBusHandler() {
    int ret;
    DBusError err;

    pipe(quitpipe);

    // initialise the error
    dbus_error_init(&err);

    // connect to the bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
        conn = 0;
        return;
    }
}
bool
DBusHandler::handle() {
    DBusMessage* msg;
    DBusError err;
    int ret;

    if (conn == 0) {
        fprintf(stderr, "Connection Null\n");
        return false;
    }

    // initialise the error
    dbus_error_init(&err);

    if (NULL == conn) {
        fprintf(stderr, "Connection Null\n");
        return false;
    }

    // request our name on the bus and check for errors
    ret = dbus_bus_request_name(conn, "vandenoever.strigi",
        DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
        fprintf(stderr, "Not Primary Owner (%d)\n", ret);
        return false;
    }

    // register the introspection interface
    DBusObjectCallHandler callhandler("/search");
    callhandler.registerOnConnection(conn);

    for (vector<DBusObjectInterface*>::iterator i = interfaces.begin();
            i != interfaces.end(); ++i) {
        callhandler.addInterface(*i);
        cerr << (*i)->getIntrospectionXML().c_str() << endl;
    }

    int fd;
    if (!dbus_connection_get_unix_fd(conn, &fd)) {
        printf("could not get connection fd\n");
    }

    // loop, testing for new messages
    fd_set rfds;
    int retval;
    struct timeval tv;
    int max = ((fd>*quitpipe) ?fd :*quitpipe)+1;
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        FD_SET(*quitpipe, &rfds);

        tv.tv_sec = 1000;
        tv.tv_usec = 0;
        retval = select(max, &rfds, NULL, NULL, &tv);
        if (retval == -1 || FD_ISSET(*quitpipe, &rfds)) { // quit
            break;
        }
        // blocking read of the next available message
        dbus_connection_read_write(conn, 0);
        while (dbus_connection_dispatch(conn) == DBUS_DISPATCH_DATA_REMAINS);
        dbus_connection_flush(conn);
    }

    // close the connection
    // these lines are commented out because they crash with dbus 0.61
    // dbus_connection_unref(conn);
    // dbus_connection_unref(conn);
    // dbus_shutdown();
    //
    return true;
}
void
DBusHandler::stop() {
    cerr << "DBusHandler::stop" << endl;
    // close the pipe to wake up the dbus thread so it can stop
    // alternatively we can write into it so we can reuse it
    // since we only use it for quitting atm, closing the pipe is fine
    close(quitpipe[1]);
}
