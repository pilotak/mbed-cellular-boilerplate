void serverDisconnect() {
    debug("Server disconnected\n");
}

bool serverConnect() {
    debug("Server connect\n");
    server_connect_id = 0;
    return true;
}

void serverReconnect() {
    if (!server_connect_id) {
        debug("Reconnecting server\n");
        bool status = serverConnect();

        if (!status) {
            debug("Reconnecting failed\n");
            server_connect_id = eQueue.call_in(5000, serverConnect);

            if (!server_connect_id) {
                debug("Calling server connect failed, no memory\n");
            }
        }

    } else {
        debug("server connect in progress\n");
    }
}
