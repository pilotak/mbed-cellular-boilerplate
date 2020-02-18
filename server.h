void serverDisconnect() {
    debug("Server disconnected\n");
}

bool serverConnect() {
    debug("Server connect\n");

    server_connect_id = 0;
    nsapi_error_t ret = NSAPI_ERROR_PARAMETER;
    TCPSocket socket;
    const char *echo_string = "TEST";
    char recv_buf[4];

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
                ret = socket.send((void*) echo_string, strlen(echo_string));

                if (ret >= NSAPI_ERROR_OK) {
                    debug("TCP: Sent %i Bytes\n", ret);
                    nsapi_size_or_error_t n = socket.recv((void*) recv_buf, sizeof(recv_buf));

                    socket.close();

                    if (n > 0) {
                        debug("Received from echo server %i bytes\n", n);
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
        return false;
    }
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
