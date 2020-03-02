bool server_done = false;

bool serverConnect() {
    debug("Server connect\n");

    nsapi_error_t ret = NSAPI_ERROR_PARAMETER;
    TCPSocket socket;
    char buffer[] = "TEST";

    SocketAddress server;
    ret = mdm->gethostbyname("echo.mbedcloudtesting.com", &server);

    if (ret == NSAPI_ERROR_OK) {
        server.set_port(7);
        ret = socket.open(mdm);

        if (ret == NSAPI_ERROR_OK) {
            debug("open OK\n");

            ret = socket.connect(server);

            if (ret == NSAPI_ERROR_OK || ret == NSAPI_ERROR_IS_CONNECTED) {
                debug("connected\n");
                ret = socket.send(buffer, sizeof(buffer));

                if (ret >= NSAPI_ERROR_OK) {
                    debug("TCP: Sent %i Bytes\n", ret);
                    memset(buffer, 0, sizeof(buffer));
                    nsapi_size_or_error_t n = socket.recv(buffer, sizeof(buffer));

                    socket.close();

                    if (n == sizeof(buffer)) {
                        debug("Received from echo server %i bytes: %.*s\n", n, sizeof(buffer), buffer);
                        return true;

                    } else {
                        debug("Socket recv FAILED: %i\n", n);
                    }

                } else {
                    debug("Socket send FAILED: %i\n", ret);
                }

            } else {
                debug("connect FAILED: %i\n", ret);
            }

        } else {
            debug("open FAILED\n");
        }

    } else {
        printf("Couldn't resolve remote host, code: %d\n", ret);
    }

    return false;
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

        } else {
            server_done = true;
        }

    } else {
        debug("server connect in progress\n");
    }
}
