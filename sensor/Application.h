//
// Created by cjalmeida on 01/06/17.
//

#ifndef PREDIX_APPLICATION_H
#define PREDIX_APPLICATION_H

#include <Poco/Util/Application.h>
#include <Poco/Util/LayeredConfiguration.h>

typedef Poco::Util::LayeredConfiguration Configuration;

/**
 * Class implementing a POCO Application.
 */
class Application : public Poco::Util::Application {
protected:

    /**
     * 'main' replacement inside the POCO framework.
     */
    int main(const std::vector<std::string> &args) override;
};

#endif //PREDIX_APPLICATION_H
