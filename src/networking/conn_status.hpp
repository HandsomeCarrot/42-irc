// Explicit open/status codes for the connection host (C++98, no exceptions).

#ifndef CONN_STATUS_HPP
#define CONN_STATUS_HPP

enum ConnStatus {
    CONN_OK = 0,
    CONN_ERR_SOCKET = 1,
    CONN_ERR_BIND = 2,
    CONN_ERR_LISTEN = 3,
    CONN_ERR_ALREADY_OPEN = 4,
    CONN_ERR_INVALID = 5
};

#endif
