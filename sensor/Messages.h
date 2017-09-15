//
// Created by cjalmeida on 01/06/17.
//

#ifndef PREDIX_MESSAGES_H
#define PREDIX_MESSAGES_H

#include <string>

// Constant indication an unsent message
const int TRANSACTION_NEW = -1;

// Message for the time-series service
struct TimeSeriesMessage {
    std::string tagname;
    int64_t timestamp;
    double value;
    int64_t transactionId = TRANSACTION_NEW;
};

// Message for the asset service
struct AssetMessage {
    std::string sensor_id;
    int64_t timestamp;
    double value;
    std::string message;
    int64_t transactionId = TRANSACTION_NEW;
};

#endif //PREDIX_MESSAGES_H
