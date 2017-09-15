//
// Created by cjalmeida on 01/06/17.
//

#ifndef PREDIX_DISPATCHER_H
#define PREDIX_DISPATCHER_H

#include "Application.h"
#include "Messages.h"
#include "WSClient.h"
#include "HTTPClient.h"
#include <memory>
#include <thread>
#include <mutex>
#include <deque>

/**
 * Class that sends message to Predix services.
 *
 * Messages are sent enqueued using the queue_* public methods and sent asynchronously
 * later.
 *
 * The queue_* methods are effectively thread-safe.
 */
class Sender {

    // The application configuration instance
    Configuration& cfg;

    // The current access_token
    std::string token;

    // Queue for time series
    std::deque<TimeSeriesMessage> tsQueue;

    // Mutex for synchronized time series access
    std::mutex tsQueueMutex;

    // Queue for asset service
    std::deque<AssetMessage> assetQueue;

    // Mutex for synchronized asset access
    std::mutex assetQueueMutex;

    // Sequential transaction_id counter
    int64_t transactionId = 0;

    // Smart pointer to the Websocket client
    std::shared_ptr<WSClient> ws;

    Poco::Logger& logger;

    /**
     * Logins/refresh the access_token
     */
    void login();

    /**
     * Opens the Websocket connection to the timeseries service
     */
    void connect();

    /**
     * Procedure to send the queued messages to timeseries service
     */
    void sendTimeseries();

    /**
     * Procedure to send the queued messages to asset service
     */
    void sendAsset();

    /**
     * Rolls-back any "sent but not confirmed" message to the "NEW" state.
     *
     * Called when any exception is thrown.
     */
    void rollbackTransaction();

    /**
     * Validates the retured HTTP status codes and, if not OK, raise the appropriate error.
     */
    void validateResponse(HTTPClient::Response r);

    /**
     * Handles the thrown exception. May reconnect and send/resend messages after a while
     * in case of recoverables errors, or abort the program.
     */
    void handleError(int err);

    /**
     * Routine to "commit" sent messages.
     */
    template <typename T>
    void commit(std::deque<T> &queue, std::mutex &mutex, int64_t &transactionId) {
        // Lock and erase commited messages
        std::unique_lock<std::mutex> lock(mutex);
        queue.erase(
                std::remove_if(queue.begin(), queue.end(),
                               [=](const T &msg) -> bool {
                                   return msg.transactionId == transactionId;
                               }), queue.end());
    }

public:

    Sender(Configuration &cfg): cfg(cfg), logger(Poco::Logger::get("Sender")) {};

    ~Sender(){

    }

    /**
     * Start the main dispatcher loop.
     *
     * Should run in a separate thread from the main process loop to avoid blocking.
     */
    void run();

    /**
     * Add an event message to be sent to the Timeseries service. The message is sent
     * asynchronously - you can assume this method does not blocks.
     *
     * This method is thread-safe.
     *
     * @param tagname A sensor/measure unique identifier.
     * @param timestamp Milliseconds since epoch (1970-01-01T00:00:00Z).
     * @param value The measured value.
     */
    void queueTimeseriesMessage(std::string tagname, int64_t timestamp, double value);

    /**
     * Add an event message to be sent to the Asset service. The message is sent
     * asynchronously - you can assume this method does not blocks.
     *
     * This method is thread-safe.
     *
     * @param sensorId A sensor/measure unique identifier.
     * @param timestamp Milliseconds since epoch (1970-01-01T00:00:00Z).
     * @param value A numeric value to associate this message with.
     * @param message The contents of the message.
     */
    void queueAssetMessage(std::string sensorId, int64_t timestamp, double value,
                           std::string message);

};


#endif //PREDIX_DISPATCHER_H
