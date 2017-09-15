//
// Created by cjalmeida on 01/06/17.
//

#ifndef PREDIX_ERRORS_H
#define PREDIX_ERRORS_H

const int ERR_OK = 0;

// Thrown on login when user credentials are invalid
const int ERR_INVALID_CREDENTIALS = 1;

// Thrown when we get a generic 4xx request. Usually unrecoverable.
const int ERR_INVALID_REQUEST = 2;

// Thrown when we get a generic 5xx request. Usually recoverable after a while.
const int ERR_SERVER_ERROR = 3;

// Thrown when the request times out. May be after a while recoverable.
const int ERR_REQUEST_TIMEOUT = 4;

// Thrown on initialization when config dir cannot be found.
const int ERR_INVALID_CONF_DIR = 5;

// Thrown when we get a 401 response when using a Bearer token. May be recoverable after
// token refresh
const int ERR_INVALID_TOKEN = 6;

// Thrown when we get a recoverable connection error.
const int ERR_CONNECTION_ERROR = 7;

// When an error/exception was detected. Unrecoverable. The throwing function should
// log the error cause.
const int ERR_GENERIC_EXCEPTION = 99;

#endif //PREDIX_ERRORS_H
