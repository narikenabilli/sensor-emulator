//
// Created by cjalmeida on 01/06/17.
//

#ifndef PREDIX_SAMPLER_H
#define PREDIX_SAMPLER_H

#include "Application.h"
#include "Sender.h"

/**
 * "Sampler" class that simulates the sampling of sensor data.
 */
class Sampler {

    // Instance of the application configuration
    Configuration &cfg;

    // Sender object
    Sender &sender;

    Poco::Logger &logger;


public:
    Sampler(Configuration &cfg, Sender &sender) :
            cfg(cfg), sender(sender),
            logger(Poco::Logger::get("Sampler")) {};

    void run();
};


#endif //PREDIX_SAMPLER_H
