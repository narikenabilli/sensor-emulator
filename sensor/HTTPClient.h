//
// Created by cjalmeida on 03/06/17.
//

#ifndef PREDIX_HTTPCLIENT_H
#define PREDIX_HTTPCLIENT_H

#include <string>
#include <unordered_map>
#include <memory>

/**
 * Simple wrapper around POCO::Net HTTP library.
 *
 * It's far from being a complete wrapper. It only has the methods needed
 * in this project.
 */
class HTTPClient {

    typedef std::unordered_map<std::string, std::string> Headers;


    std::string uri;
    std::string requestBody;
    Headers headers;
    std::pair<std::string, std::string> basicAuth;

    uint64_t timeout = 0;

public:

    static const std::string CT_JSON;
    static const std::string CT_FORM;

    HTTPClient(std::string uri);

    void setHeader(std::string key, std::string value);

    void setBasicAuth(std::string username, std::string password);

    void setContentType(std::string contentType);

    void setBody(const std::string &body);

    void setTimeout(uint64_t timeout);

    enum ErrorCode {
        OK = 0,
        GENERIC_SSL_ERROR = 1,
        TIMEOUT_ERROR = 2,
        CONNECTION_ERROR = 3,
        UNKNOWN_ERROR = 99
    };

    struct Response {
        std::string text;
        std::string error_message;
        Headers headers;
        int status_code;
        ErrorCode error_code;
    };

    Response post();
};

class FormBody {
    std::unordered_map<std::string, std::string> formParams;
public:

    void set(std::string key, std::string value);

    std::string toString();
};


#endif //PREDIX_HTTPCLIENT_H
