//
// Created by cjalmeida on 02/06/17.
//

#ifndef PREDIX_WSCLIENT_H
#define PREDIX_WSCLIENT_H

#include <string>
#include <unordered_map>
#include <memory>
#include <Poco/Net/WebSocket.h>
#include <Poco/Logger.h>

/**
 * Facade wrapping the POCO websocket library for simplicity.
 */
class WSClient {

    // The target URI
    std::string uri;

    // Map with additional headers
    std::unordered_map<std::string, std::string> headersMap;

    // Pointer to the underlying websocket connection
    std::shared_ptr<Poco::Net::WebSocket> ws;

    // Send timeout in millis
    long sendTimeout;

    // Recv timeout in millis
    long recvTimeout;

    Poco::Logger& logger;

public:
    // Initialize the ws client
    WSClient(std::string uri);

    // Connect to the remote service
    void connect();

    // Sets an additional header value
    void setHeader(std::string key, std::string value);

    // Sends a TEXT frame (blocking)
    void sendText(std::string text);

    // Receives a TEXT frame (blocking)
    std::string receiveText();

    // Timeout when sending
    void setSendTimeout(long ms);

    // Timeout when receiving
    void setRecvTimeout(long ms);

    void checkConnected();


};


#endif //PREDIX_WSCLIENT_H
