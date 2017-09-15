//
// Created by cjalmeida on 03/06/17.
//

#include "HTTPClient.h"
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Base64Encoder.h>
#include <sstream>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <iostream>

using namespace std;
using namespace Poco::Net;

const string HTTPClient::CT_JSON = "application/json";
const string HTTPClient::CT_FORM = "application/x-www-form-urlencoded";

HTTPClient::HTTPClient(std::string uri): uri(uri) {
}

void HTTPClient::setHeader(std::string key, std::string value) {
    headers[key] = value;
}

void HTTPClient::setBasicAuth(std::string username, std::string password) {
    this->basicAuth = {username, password};
}

void HTTPClient::setContentType(std::string contentType) {
    setHeader("Content-Type", contentType);
}

void HTTPClient::setBody(const std::string &body) {
    requestBody = body;
}

void HTTPClient::setTimeout(uint64_t timeout) {
    this->timeout = timeout;
}

HTTPClient::Response HTTPClient::post() {
    const Poco::URI uri(this->uri);

    string path = uri.getPathAndQuery();
    string host = uri.getHost();
    uint16_t port = uri.getPort();

    HTTPRequest req(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPRequest::HTTP_1_1);
    HTTPResponse res;

    // set headers
    req.set("Host", host);
    for (auto& item : headers) {
        req.set(item.first, item.second);
    }

    if (!basicAuth.first.empty()) {
        HTTPBasicCredentials cred(basicAuth.first, basicAuth.second);
        cred.authenticate(req);
    }
    req.setContentLength(requestBody.length());
    std::ostringstream bodystream;
    HTTPClientSession* session;
    Response r;
    try {
        if (uri.getScheme() == "http") {
            session = new HTTPClientSession(host, port);
        } else {
            session = new HTTPSClientSession(host, port);
        }

        if (timeout) session->setTimeout(Poco::Timespan(0, timeout * 1000));
        auto s = bodystream.str();
        session->sendRequest(req) << requestBody;
        bodystream << session->receiveResponse(res).rdbuf();
        r.status_code = res.getStatus();
        r.error_message = res.getReason();
        r.text = bodystream.str();
        r.error_code = OK;
        for (auto it = res.begin(); it != res.end(); it++)
            r.headers[it->first] = it->second;

        delete session;

    } catch (Poco::Exception ex) {
        if (ex.className() == "TimeoutException") {
            r.error_code = ErrorCode::TIMEOUT_ERROR;
        } else {
            r.error_code = ErrorCode::UNKNOWN_ERROR;
        }
        r.error_message = ex.message();
        r.status_code = 0;
    }

    return std::move(r);
}

void FormBody::set(string key, string value) {
    formParams[key] = value;
}

std::string FormBody::toString() {
    Poco::Net::HTMLForm form;
    for (auto& item : formParams) {
        form.set(item.first, item.second);
    }

    std::ostringstream str;
    form.write(str);
    return str.str();
}
