//
// Created by cjalmeida on 02/06/17.
//

#include "WSClient.h"

#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPStreamFactory.h>
#include <Poco/Net/HTTPSStreamFactory.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Logger.h>
#include "errors.h"
using namespace Poco::Net;
using namespace std;

void WSClient::connect() {

    // Using POCO libs to handle websocket connection
    Poco::URI uri(this->uri);
    string host = uri.getHost();
    uint16_t port = uri.getPort();
    string path = uri.getPath();

    HTTPSClientSession cs(host, port);
    HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    for (auto& item : headersMap) {
        request.set(item.first, item.second);
    }
    HTTPResponse response;
    try {
        ws = make_shared<WebSocket>(cs, request, response);
        ws->setKeepAlive(true);
        ws->setBlocking(true);
    } catch (Poco::Exception ex) {
        if (ex.code() == WebSocket::WS_ERR_UNAUTHORIZED) {
            throw ERR_INVALID_TOKEN;
        } else {
            logger.error("Unexpected error connecting to websocket client. Cause: %s", ex.message());
            // An error here is unexpected.
            throw ERR_GENERIC_EXCEPTION;
        }
    }
}

void WSClient::setHeader(std::string key, std::string value) {
    headersMap[key] = value;
}

WSClient::WSClient(std::string uri): uri(uri), logger(Poco::Logger::get("WSClient")) {

}

void WSClient::sendText(std::string text) {
    try {
        ws->setSendTimeout(Poco::Timespan(0, sendTimeout * 1000));
        ws->sendFrame(text.c_str(), (int) text.size());
    } catch (Poco::TimeoutException ex) {
        logger.warning("Timed-out when sending TEXT frame");
        throw ERR_REQUEST_TIMEOUT;
    } catch (Poco::Exception ex) {
				logger.warning("Error sending TEXT frame. Reason: " + ex.message());
				throw ERR_CONNECTION_ERROR;
		}
}

std::string WSClient::receiveText() {
    char buf[4096];
    int flags;
    try {
        ws->setReceiveTimeout(Poco::Timespan(0, recvTimeout * 1000));
        int n = ws->receiveFrame(buf, sizeof(buf), flags);
        if (flags & WebSocket::FRAME_OP_TEXT) {
            string recv(buf, (unsigned long) n);
            return recv;
        } else {
            logger.error("Invalid frame type. Flags: %d", flags);
            // Anything but a text frame here is undefined behavior.
            throw ERR_INVALID_REQUEST;
        }
    } catch (Poco::TimeoutException ex) {
        logger.warning("Timed-out when waiting TEXT frame");
        throw ERR_REQUEST_TIMEOUT;
    }
}

void WSClient::setSendTimeout(long ms) {
    this->sendTimeout = ms;
}

void WSClient::setRecvTimeout(long ms) {
    this->recvTimeout = ms;
}

void WSClient::checkConnected() {
    if (!ws) {
        throw std::runtime_error("Please call 'connect()' first.");
    }
}
