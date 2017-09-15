PREDIX SENSOR EMULATION IN C++
==============================


System requirements
-------------------

The application should build and run in any modern version of Window, Linux and MacOS with a
suitable compiler toolchain installed.

### Linux

Use your distro package manager to install the following applications:

  * Python 2.7 (ie. 'python')
  * Python PIP (ie. 'python pip')
  * CMake 3.5 or later (ie. 'cmake')
  * GNU GCC/Make toolchain (ie 'build-essential')

You'll need the Python local installation directory folder in PATH. You may run the following
to make it happen:

    $ export PATH="$HOME/.local/bin/:$PATH"

### Windows

Download and install the following applications:
  
  * MS Visual Studio 2017 Community Edition ([download link](https://www.visualstudio.com/downloads/))
  * CMake 3.5 or later ([download link](https://cmake.org/download/))
  * Python 2.7 ([download link](https://www.python.org/downloads/windows/))


The older versions of Python 2.7 does not have the required `pip` tool. Make sure you have
by checking the `<python_install>\Scripts\pip.exe` file. 

The `cmd` commands in the following sections assume you are using the 
`Developer Command Prompt for VS 2017`. Also you have the Python install and script folders added 
to the `PATH` environment variable. To do this for a `cmd.exe` instance, assuming default Python 
installation dir:

    > set PATH=c:\Python27;c:\Python27\Scripts;%PATH%

### MacOS X

Download and install the following applications. If you prefer, you may use Homebrew or your 
prefered package manager:

  * XCode 8.3 (from AppStore, may work with earlier versions)
  * CMake 3.5 or later ([download link](https://cmake.org/download/))
  * Python 2.7 ([download link](https://www.python.org/downloads/windows/))
  * Python pip (`sudo easy_install pip`)

MacOS X Python does not comes with `pip` by default. You can use the `easy_install` tool to
bootstrap it. Or, alternatively, you may use Homebrew's `python` package.

You'll need the Python local installation directory folder in PATH. You may run the following
to make it happen:

    $ export PATH="$HOME/Library/Python/2.7/bin/:$PATH"

Deployment
----------

### Predix setup

This demo requires the activation and setup of a number of Predix services. To simplify the task,
you can follow the short steps below fully setup the Predix services and collect the necessary
info into the configuration files. (Note: in Windows, adapt where necessary)
    
    (in <project_root> folder)

    $ pip install -r setup/requirements.txt
    $ python setup/predix_setup.py -a -u <email> -p <password>

In the command above, replace `<email>` by your Predix.io account email and `<password>` by your
account password. After a while, all services are created and configured using the default values 
and the needed runtime information is written to `conf/sensor.ini`.


### Building

We use [Conan.io](http://conan.io) to manage all dependencies and CMake to create the cross-platform
build setup. The `conan` tool is installed as part of the "Predix setup" `pip` install and should 
be available. To build: (in Windows, adapt where necessary)

    (in <project_root> folder)

    $ mkdir build
    $ cd build
    $ conan install .. --build missing
    $ conan build ..
    $ cd ..

The first `conan` command will download and build (if needed) the project dependencies from the 
Conan.io central repository. It may take a while. The second one will build the sensor emulator 
program, output in `<project_root>/build/bin/sensor`


### Running

After compilation, run the following:

    (in <project_root> folder)

    $ build/bin/sensor

Note: the application *require* the <project_root> as the "current working directory" when starting.

The application should log it's behavior (in DEBUG level, by default). You may change the 
parameters (*p*, *m*, *dt*) in the `conf/sensor.ini` file.



Setup details
-------------

The `predix_setup.py` script essentially does:

  * Create new application in your Predix "dev" space.
  * Create and bind the `User Account and Authentication (UAA)` service to the app.
  * Create and bind the `Timeseries` service to the app.
  * Create and bind the `Assets` service to the app.
  * Create users in UAA for server and the sensor emulator
  * Write `conf/sensor.ini` and `conf/server.ini` config files with the relevant service data.