//
// Created by cjalmeida on 01/06/17.
//

#include "Application.h"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPSStreamFactory.h>
#include <Poco/Net/HTTPStreamFactory.h>
#include <Poco/URI.h>
#include <Poco/File.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/Logger.h>
#include <string>
#include <iostream>
#include "errors.h"
#include "Sampler.h"
#include <thread>

using namespace std;
using namespace Poco::Net;
using namespace Poco;

void setupLogging(string level) {
    AutoPtr<ConsoleChannel> console(new ConsoleChannel);
    AutoPtr<PatternFormatter> pattFormatter(new PatternFormatter);
    pattFormatter->setProperty("pattern", "%Y-%m-%d %H:%M:%S %q [%s] %t");
    AutoPtr<FormattingChannel> formattingChannel(new FormattingChannel(pattFormatter, console));
    Logger::root().setChannel(formattingChannel);
    Logger::root().setLevel(level);
}

int Application::main(const std::vector<std::string> &args) {

    string confdir("conf");

    if (args.size() > 0) {
        confdir = args[0];
    }

    // Check we're in the correct directory
    Poco::File iniFile(Poco::Path(confdir, "sensor.ini"));
		bool ini_ok = true;
		try {
			ini_ok = iniFile.canRead();
		}
		catch (Poco::Exception ex) {
			ini_ok = false;
		}

		if (!ini_ok) {
			cerr << "ERROR: Cannot read " << iniFile.path() << ". Make sure the you passed the 'conf' "
				"directory as the sole argument of the program." << endl;
			exit(ERR_INVALID_CONF_DIR);
		}

    // Load configuration
    loadConfiguration(iniFile.path());

    setupLogging(config().getString("logging.loglevel", "information"));

    // Setup and initialize SSL
    auto certs = Path(confdir, "ca-certificates.crt").absolute().toString();
    config().setString("openSSL.client.caConfig", certs);
    Poco::Net::initializeSSL();
    HTTPStreamFactory::registerFactory();
    HTTPSStreamFactory::registerFactory();

    // Setup sender and sampler objects
    Sender sender(config());
    Sampler sampler(config(), sender);

    // Start a thread for each object
    thread samplerThread([&sampler]() {
        sampler.run();
    });

    thread senderThread([&sender](){
        sender.run();
    });

    samplerThread.join();
    senderThread.join();

    return 0;
}

POCO_APP_MAIN(Application);