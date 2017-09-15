//
// Created by cjalmeida on 01/06/17.
//

#include "Sampler.h"
#include <string>
#include <random>
#include <thread>
#include <iostream>

using namespace std;

int64_t get_current_time_ms() {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return ms.count();
}

void Sampler::run() {
    // get configuration parameters
    double p = cfg.getDouble("sensor.p");
    double m = cfg.getDouble("sensor.m");
    int64_t dt = (int64_t) (cfg.getDouble("sensor.dt") * 1000.0);
    string deviceUUID = cfg.getString("sensor.client_id");

    // Get a pseudo-RNG to simulate data sampling
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    string assetContent{"ERROR: Sensor overloaded"};

    // Main loop. Sends messages to asset or time-series services according to challenge rule.
    for (;;) {
        double rnd = dist(gen);
        int64_t timestamp = get_current_time_ms();
        if (rnd < p + m) {
            poco_debug_f1(logger, "TS: %.5f", rnd);
            sender.queueTimeseriesMessage(deviceUUID, timestamp, rnd);
        }
        if (rnd < m) {
            poco_debug_f1(logger, "Asset: %.5f", rnd);
            sender.queueAssetMessage(deviceUUID, timestamp, rnd, assetContent);
        }

        if (rnd >= p + m) {
            poco_debug_f1(logger, "NOOP: %.5f", rnd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(dt));
    }
}

