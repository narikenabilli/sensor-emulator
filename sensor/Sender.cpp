//
// Created by cjalmeida on 01/06/17.
//

#include "Sender.h"
#include <nlohmann/json.hpp>
#include "errors.h"
#include "HTTPClient.h"
#include <Poco/UUIDGenerator.h>

#define REQUEST_TIMEOUT_MS 10000
#define DISPATCH_SLEEP_TIME_MS 100
#define ERROR_SLEEP_MS 5000

using namespace std;
using namespace nlohmann;

void Sender::run() {

    for (;;) {
        try {
            // Outer loop to handle re-connection
            login();
            connect();

            for (;;) {
                // Send messages in queues
                sendTimeseries();
                sendAsset();

                // Sleep for a while to avoid busy-wait
                this_thread::sleep_for(chrono::milliseconds(DISPATCH_SLEEP_TIME_MS));
            }
        } catch (int err) {

            // rollback any pending message sending transaction
            rollbackTransaction();

            // handle known errors appropriately
            handleError(err);
        }
    }


}

void Sender::queueTimeseriesMessage(std::string tagname, int64_t timestamp, double value) {
    std::unique_lock<mutex> lock(tsQueueMutex);
    TimeSeriesMessage msg;
    msg.tagname = tagname;
    msg.timestamp = timestamp;
    msg.value = value;
    tsQueue.push_back(msg);
}

void Sender::queueAssetMessage(std::string sensorId, int64_t timestamp, double value,
                               std::string message) {
    std::unique_lock<mutex> lock(assetQueueMutex);
    AssetMessage msg;
    msg.sensor_id = sensorId;
    msg.timestamp = timestamp;
    msg.value = value;
    msg.message = message;
    assetQueue.push_back(msg);
}

void Sender::connect() {

    logger.information("Connecting to the TS WebSocket");
    string ts_uri = cfg.getString("timeseries.ingest_uri");
    string zone_id = cfg.getString("timeseries.zone_id");
    string client_id = cfg.getString("sensor.client_id");

    // set appropriate headers
    ws = make_shared<WSClient>(ts_uri);
    ws->setHeader("Authorization", "Bearer " + token);
    ws->setHeader("Predix-Zone-Id", zone_id);
    ws->setHeader("Origin", "sensor://" + client_id);
    ws->connect();
    ws->setSendTimeout(REQUEST_TIMEOUT_MS);
    ws->setRecvTimeout(REQUEST_TIMEOUT_MS);
    logger.information("Connected!");
}


void Sender::sendTimeseries() {
    // No need to lock for now, we're not messing with the queue structure.

    if (tsQueue.size() == 0)
        return;

    logger.debug("Sending %d messages to the TIMESERIES service.", (int) tsQueue.size());

    transactionId++;
    // First we group the data by tagname assuming we're getting more than one class of data
    unordered_map<string, vector<TimeSeriesMessage *>> data;
    for (size_t i = 0, n = tsQueue.size(); i < n; i++) {
        auto &msg = tsQueue[i];
        if (msg.transactionId == TRANSACTION_NEW) {
            msg.transactionId = transactionId;
            data[msg.tagname].push_back(&msg);
        }
    }

    // Prepare message body
    json body = json::array();
    for (auto &item : data) {
        auto tagname = item.first;
        auto msglist = item.second;
        json datapoints = json::array();
        for (size_t i = 0, n = msglist.size(); i < n; i++) {
            auto msg = msglist[i];
            datapoints.push_back({msg->timestamp, msg->value, 3});
        }
        body.push_back({{"name",       tagname},
                        {"datapoints", datapoints}});
    }

    // Prepare payload message
    string messageId = "msg-" + to_string(transactionId);
    json payload = {{"messageId", messageId},
                    {"body",      body}};

    // Send message and receive confirmation
    auto sendtxt = payload.dump();
    ws->sendText(sendtxt);
    auto recvtxt = ws->receiveText();
    auto recv = json::parse(recvtxt);
    int code = recv["statusCode"];
    if (code >= 200 && code <= 299) {
        assert(recv["messageId"] == messageId);
        commit(tsQueue, tsQueueMutex, transactionId);
    } else {
        // An invalid status code is not expected.
        throw ERR_GENERIC_EXCEPTION;
    }
}

void Sender::sendAsset() {
    transactionId++;

    if (assetQueue.size() == 0)
        return;

    logger.debug("Sending %d messages to the ASSET service.", (int) assetQueue.size());

    auto base_uri = cfg.getString("asset.uri");
    auto zone_id = cfg.getString("asset.zone_id");
    auto collection = cfg.getString("asset.collection");
    auto post_uri = base_uri + collection;

    // Create an object for each message
    json body = json::array();
    for (auto &msg: assetQueue) {
        if (msg.transactionId == TRANSACTION_NEW) {
            msg.transactionId = transactionId;
            auto uuid = Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
            string uri = collection + '/' + uuid;
            json value = {
                    {"uri",       uri},
                    {"sensor_id", msg.sensor_id},
                    {"timestamp", msg.timestamp},
                    {"val",       msg.value},
                    {"msg",       msg.message},
            };
            body.push_back(value);
        }
    }

    // create the POST request
    auto client = HTTPClient(post_uri);
    client.setHeader("Authorization", "Bearer " + token);
    client.setHeader("Predix-Zone-Id", zone_id);
    client.setBody(body.dump());
    client.setTimeout(REQUEST_TIMEOUT_MS);
    client.setContentType(HTTPClient::CT_JSON);
    auto r = client.post();

    validateResponse(r);

    // synchronized code to remove sent messages
    commit(assetQueue, assetQueueMutex, transactionId);

}

void Sender::login() {
    logger.information("Fetching OAuth access token.");

    // Fetch access_token for this client
    string uri = cfg.getString("uaa.uri");
    string client_id = cfg.getString("sensor.client_id");
    string client_secret = cfg.getString("sensor.client_secret");
    uri.append("/oauth/token");

    auto client = HTTPClient(uri);
    client.setBasicAuth(client_id, client_secret);
    client.setTimeout(REQUEST_TIMEOUT_MS);
    auto form = FormBody();
    form.set("response_type", "token");
    form.set("grant_type", "client_credentials");
    client.setBody(form.toString());
    client.setContentType(HTTPClient::CT_FORM);
    auto r = client.post();

    validateResponse(r);

    auto data = json::parse(r.text);
    this->token = data["access_token"];
    logger.information("Got OAuth access token.");
}

void Sender::rollbackTransaction() {
    logger.information("Rolling back transaction.");
    {
        std::unique_lock<mutex> lock(tsQueueMutex);
        for (auto &msg : tsQueue) msg.transactionId = TRANSACTION_NEW;
    }

    {
        std::unique_lock<mutex> lock(assetQueueMutex);
        for (auto &msg : assetQueue) msg.transactionId = TRANSACTION_NEW;
    }
}

void Sender::validateResponse(HTTPClient::Response r) {
    typedef HTTPClient::ErrorCode E;

    auto err = r.error_code;

    if (err == E::OK) {
        int status = r.status_code;

        if (status >= 200 && status <= 299) {
            return;
        } else if (status == 401 || status == 403) {
            throw ERR_INVALID_CREDENTIALS;
        } else if (status >= 400 && status <= 499) {
            throw ERR_INVALID_REQUEST;
        } else if (status >= 500 && status <= 500) {
            throw ERR_SERVER_ERROR;
        } else {
            logger.error("Unknown HTTP status: %d", status);
            throw ERR_GENERIC_EXCEPTION;
        }
    } else if (err == E::GENERIC_SSL_ERROR) {
        // Some errors that are known to be unrecoverable.
        logger.error("Unrecoverable connection error: %s", r.error_message);
        throw ERR_GENERIC_EXCEPTION;
    } else {
        // Connection errors should be recoverable after a while
        throw ERR_CONNECTION_ERROR;
    }
}

void Sender::handleError(int err) {
    auto warn_sleep = [&](string msg, int64_t sleep_time) {
        logger.warning("%s, Sleeping for %ld ms and trying again.", msg, (long) sleep_time);
        this_thread::sleep_for(chrono::milliseconds(sleep_time));
    };

    auto fail = [this, &err](string msg) {
        logger.error("%s. Exiting.", msg);
        exit(err);
    };

    if (err == ERR_INVALID_CREDENTIALS) {
        fail("Invalid credentials.");
    } else if (err == ERR_INVALID_REQUEST) {
        fail("Invalid request.");
    } else if (err == ERR_REQUEST_TIMEOUT) {
        warn_sleep("Timeout when performing request.", ERROR_SLEEP_MS);
    } else if (err == ERR_SERVER_ERROR) {
        warn_sleep("Server error.", ERROR_SLEEP_MS);
    } else if (err == ERR_CONNECTION_ERROR) {
        warn_sleep("Connection error.", ERROR_SLEEP_MS);
    } else if (err == ERR_INVALID_TOKEN) {
        warn_sleep("Server error.", 0);
    } else {
        fail("Unexpected error");
    }
}