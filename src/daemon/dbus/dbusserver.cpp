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
#include "dbusserver.h"
#include "interface.h"
#include "dbusobjectcallhandler.h"
#include "dbusclientinterface.h"
#include "testinterface.h"
#include "dbustestinterface.h"
using namespace std;

void*
DBusServer::run(void*) {
    DBusConnection* conn;
    DBusMessage* msg;
    DBusError err;
    int ret;

    printf("Listening for method calls\n");
    pipe(quitpipe);

    // initialise the error
    dbus_error_init(&err);

    // connect to the bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
     //   return false;
    }
    if (NULL == conn) {
        fprintf(stderr, "Connection Null\n");
    //    return false;
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
   //     return false;
    }

    // register the introspection interface
    DBusObjectCallHandler callhandler("/search");
    callhandler.registerOnConnection(conn);

    DBusClientInterface searchinterface(interface);
    if (interface) {
        callhandler.addInterface(&searchinterface);
    }
    printf("%s\n", searchinterface.getIntrospectionXML().c_str());

    TestInterface test;
    DBusTestInterface testinterface(&test);
    callhandler.addInterface(&testinterface);

    printf("%s\n", callhandler.getIntrospectionXML().c_str());

    // loop, testing for new messages
    while ((!interface || interface->isActive()) && getState() != Stopping) {
        // blocking read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        dbus_connection_flush(conn);
    }

    // close the connection
    dbus_connection_unref(conn);
    dbus_shutdown();
    return &thread;
}
