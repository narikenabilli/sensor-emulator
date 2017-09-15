#
# Copyright 2017, TopCoder Inc. All rights reserved.
#

from __future__ import print_function, unicode_literals

import sys
import platform
import os
import json
import requests
import base64
import binascii
import argparse
import uuid
import time
from ConfigParser import ConfigParser
from clint.textui import puts, prompt, colored
from getpass import getpass
from conans import tools
from subprocess import check_output, check_call, CalledProcessError

welcome_msg = """
This script will setup the needed Predix services. You need your own Predix.io account
to continue.
"""

# Predix user regions
REGIONS = [{'selector': '1', 'prompt': 'US-West',
            'return': 'https://api.system.aws-usw02-pr.ice.predix.io'},
           {'selector': '2', 'prompt': 'US-East',
            'return': 'https://api.system.asv-pr.ice.predix.io'},
           {'selector': '3', 'prompt': 'Japan',
            'return': 'https://api.system.aws-jp01-pr.ice.predix.io'}]

# Cloundfoundry CLI download locations
CF_CLI = {
    'Linux': (
        'https://cli.run.pivotal.io/stable?release=linux64-binary&source=github', 'bin/cf.tgz'),
    'Darwin': (
        'https://cli.run.pivotal.io/stable?release=macosx64-binary&source=github',
        'bin/cf.tgz'),
    'Windows': (
        'https://cli.run.pivotal.io/stable?release=windows64-exe&source=github', 'bin/cf.zip'),
}

SETUP_DEFAULTS = {
    'space': 'dev',
    'app_name': 'tc-predix',
    'asset_name': 'topcoder-asset',
    'asset_plan': 'Free',
    'sensor_client_id': str(uuid.uuid4()),
    'sensor_server_id': 'mock_server',
    'uaa_name': 'topcoder-uaa',
    'uaa_plan': 'Free',
    'uaa_subdomain': 'topcoder',
    'ts_name': 'topcoder-ts',
    'ts_plan': 'Free',
    'asset_collection': '/sensor-logs'
}

PARAMS_DEFAULTS = {
    'p': 0.60,
    'm': 0.05,
    'dt': 1.0
}

THIS = os.path.abspath(__file__)


class PredixSetup(object):
    def __init__(self):
        self.args = None  # type: argparse.Namespace
        self.auto = False
        self.clean = False
        self.username = None  # type: str
        self.password = None  # type: str
        self.app_name = None  # type: str
        self.app_guid = None  # type: str
        self.uaa_name = None  # type: str
        self.uaa = None  # type: dict
        self.ts_name = None  # type: str
        self.ts = None  # type: dict
        self.asset_name = None  # type: str
        self.asset = None  # type: dict
        self.admin_access_token = None  # type:str
        self.user_device = None  # type: dict
        self.user_server = None  # type: dict

    def configure(self):
        parser = argparse.ArgumentParser(
            description="Setup a Predix.io environment for the demo project."
        )

        parser.add_argument('--auto', '-a', action='store_true',
                            help='Non-interactive setup using defaults.')
        parser.add_argument('--username', '-u', help='The account username. Required if -a.')
        parser.add_argument('--password', '-p', help='The account password. Required if -a.')
        parser.add_argument('--clean', action='store_true',
                            help='Cleanup predix environment using defaults')

        self.args = parser.parse_args()

        if self.args.auto:
            puts('')
            puts(colored.yellow("*** Non-interactive mode using defaults ***"))
            puts('')

            if not self.args.username or not self.args.password:
                puts(colored.red(
                    "username/password are required when running non-interactively."
                ))
                sys.exit(1)

            self.auto = self.args.auto
            self.username = self.args.username
            self.password = self.args.password

        if self.args.clean:
            self.clean = True

    def run(self):
        puts(colored.cyan(welcome_msg))
        self.configure()
        self.predix_login()

        if self.clean:
            self.predix_cleanup()

        if (self.clean and self.auto) or not self.clean:
            self.create_application()
            self.create_uaa_service()
            self.create_timeseries_service()
            self.create_asset_service()
            self.uaa_admin_login()
            self.uaa_add_clients()
            self.post_asset_model()
            self.write_predix_ini()
            self.cf_restage()
            self.puts_success()

    def predix_cleanup(self):
        cmds = (
            [cf(), 'delete', '-r', '-f', SETUP_DEFAULTS['app_name']],
            [cf(), 'ds', '-f', SETUP_DEFAULTS['uaa_name']],
            [cf(), 'ds', '-f', SETUP_DEFAULTS['ts_name']],
            [cf(), 'ds', '-f', SETUP_DEFAULTS['asset_name']],
        )
        for cmd in cmds:
            try:
                check_call(cmd)
            except:
                pass

    def create_asset_service(self):
        msg = colored.cyan("\nWe will now create the ")
        msg += colored.cyan("Predix Asset", bold=True)
        msg += colored.cyan(" service.")
        puts(msg)

        if not self.auto:
            msg = colored.yellow("The default answers for the following questions should be fine.")
            puts(msg)

        self.asset_name = self.auto_query("Service instance ID:", SETUP_DEFAULTS['asset_name'])
        plan = self.auto_query("Service plan:", SETUP_DEFAULTS['asset_plan'])

        # Creating the service
        params = {'trustedIssuerIds': [self.uaa['credentials']['issuerId']]}
        params = json.dumps(params)
        check_call([cf(), 'create-service', 'predix-asset', plan, self.asset_name, '-c', params])

        # Binding the service
        check_call([cf(), 'bind-service', self.app_name, self.asset_name])

        # get ts service data
        for asset in self.cf_env()['system_env_json']['VCAP_SERVICES']['predix-asset']:
            if asset['name'] == self.asset_name:
                self.asset = asset

    def write_predix_ini(self):
        cfg_set = (('server', 'server/server.ini'), ('sensor', 'conf/sensor.ini'))

        for cfg_type, outfile in cfg_set:
            cfg = ConfigParser()
            if cfg_type == 'sensor':
                cfg.add_section('sensor')
                cfg.set('sensor', 'p', PARAMS_DEFAULTS['p'])
                cfg.set('sensor', 'm', PARAMS_DEFAULTS['m'])
                cfg.set('sensor', 'dt', PARAMS_DEFAULTS['dt'])
                cfg.set('sensor', 'client_id', self.user_device['client_id'])
                cfg.set('sensor', 'client_secret', self.user_device['client_secret'])

            if cfg_type == 'server':
                cfg.add_section('server')
                cfg.set('server', 'client_id', self.user_server['client_id'])
                cfg.set('server', 'client_secret', self.user_server['client_secret'])

            cfg.add_section('logging')
            cfg.set('logging', 'loglevel', 'debug')

            cfg.add_section('uaa')
            cfg.set('uaa', 'uri', self.uaa['credentials']['uri'])

            cfg.add_section('timeseries')
            cfg.set('timeseries', 'ingest_uri', self.ts['credentials']['ingest']['uri'])
            cfg.set('timeseries', 'query_uri', self.ts['credentials']['query']['uri'])
            cfg.set('timeseries', 'zone_id',
                    self.ts['credentials']['ingest']['zone-http-header-value'])

            cfg.add_section('asset')
            cfg.set('asset', 'uri', self.asset['credentials']['uri'])
            cfg.set('asset', 'zone_id', self.asset['credentials']['zone']['http-header-value'])
            cfg.set('asset', 'collection', SETUP_DEFAULTS['asset_collection'])

            with open(outfile, 'w') as f:
                cfg.write(f)

    def puts_success(self):
        msg = colored.magenta(
            "\nSUCCESS! Your environment was setup and the configuration files were created"
        )
        puts(msg)

    def uaa_admin_login(self):
        puts(colored.cyan("\nLogging in to UAA as 'admin' to get access token."))
        try:
            self.admin_access_token = self.uaa_login('admin', self.password)
        except:
            puts(colored.red(
                "Error: Could not login to UAA as 'admin'. Have you changed the service password?"))
            sys.exit(1)

    def uaa_login(self, client_id, client_secret):
        base = self.uaa['credentials']['uri']
        url = base + '/oauth/token'
        params = {
            'response_type': 'token',
            'grant_type': 'client_credentials'
        }
        headers = basic_auth(client_id, client_secret)
        r = requests.post(url, data=params, headers=headers)
        if not r.ok:
            raise Exception('Login error')
        data = r.json()
        return data['access_token']

    def uaa_add_clients(self):
        """
        Creates the required UAA clients using the UAA API.
        """
        msg = colored.cyan(
            "\nWe're going to create '%s' and '%s' users with "
            "random generated passwords. Please wait a minute." %
            (SETUP_DEFAULTS['sensor_client_id'], SETUP_DEFAULTS['sensor_server_id'])
        )
        puts(msg)

        url = self.uaa['credentials']['uri'] + '/oauth/clients'
        ts_zone_id = self.ts['credentials']['ingest']['zone-http-header-value']
        asset_zone_id = self.asset['credentials']['zone']['http-header-value']
        headers = bearer_auth(self.admin_access_token)

        genpass = lambda: binascii.b2a_base64(os.urandom(18)).strip()
        self.user_device = dict(client_id=SETUP_DEFAULTS['sensor_client_id'],
                                client_secret=genpass())
        self.user_server = dict(client_id=SETUP_DEFAULTS['sensor_server_id'],
                                client_secret=genpass())

        for user, scope in ((self.user_device, 'ingest'), (self.user_server, 'query')):
            params = {
                'client_id': user['client_id'],
                'client_secret': user['client_secret'],
                'authorized_grant_types': 'client_credentials',
                'authorities': ['timeseries.zones.%s.user' % ts_zone_id,
                                'timeseries.zones.%s.%s' % (ts_zone_id, scope),
                                'predix-asset.zones.%s.user' % asset_zone_id]
            }
            r = requests.post(url, json=params, headers=headers)
            if r.ok:
                puts("Created UAA client: %s" % user['client_id'])
            else:
                puts(colored.red("Error creating UAA user: %s" % user['client_id']))
                sys.exit(1)

    def post_asset_model(self):
        msg = colored.cyan("\nCreating asset data model. Please wait a minute.")
        puts(msg)

        url = self.asset['credentials']['uri'] + SETUP_DEFAULTS['asset_collection']
        zone_id = self.asset['credentials']['instanceId']
        data = [
            {
                'uri': SETUP_DEFAULTS['asset_collection'] + '/' + str(uuid.uuid4()),
                'sensor_id': SETUP_DEFAULTS['sensor_client_id'],
                'timestamp': int(time.time() * 1000),
                'val': 0,
                'msg': '--- START OF HISTORY ---'
            }
        ]

        # login as server UAA client
        token = self.uaa_login(self.user_server['client_id'], self.user_server['client_secret'])
        headers = bearer_auth(token)
        headers['Predix-Zone-Id'] = zone_id
        r = requests.post(url, json=data, headers=headers)
        if r.ok:
            puts("Created asset data model.")
        else:
            puts(colored.red("Error creating asset data model."))
            sys.exit(1)

    def create_application(self):
        msg = colored.cyan("\nWe will now create an application. ")
        puts(msg)

        self.app_name = self.auto_query("Application name:", SETUP_DEFAULTS['app_name'])
        self.cf_push()

        # retrieve app_guid
        entities = cf_curl('/v2/apps')['resources']
        for app in entities:
            if app['entity']['name'] == self.app_name:
                self.app_guid = app['metadata']['guid']

    def create_uaa_service(self):
        msg = colored.cyan("\nWe will now create the required ")
        msg += colored.cyan("User Account and Authentication", bold=True)
        msg += colored.cyan(" service.")
        puts(msg)

        if not self.auto:
            msg = colored.yellow("The default answers for the following questions should be fine. "
                                 "The 'admin' secret will be the same as your account password.")
            puts(msg)

        self.uaa_name = self.auto_query("Service instance ID:", SETUP_DEFAULTS['uaa_name'])
        plan = self.auto_query("Service plan:", SETUP_DEFAULTS['uaa_plan'])
        subdomain = self.auto_query("Service subdomain:", SETUP_DEFAULTS['uaa_subdomain'])

        # Creating the service
        params = {"adminClientSecret": self.password, "subdomain": subdomain}
        params = json.dumps(params)
        check_call(
            [cf(), 'create-service', 'predix-uaa', plan, self.uaa_name, '-c', params])

        # Binding the service
        check_call([cf(), 'bind-service', self.app_name, self.uaa_name])

        # get uaa service data
        for uaa in self.cf_env()['system_env_json']['VCAP_SERVICES']['predix-uaa']:
            if uaa['name'] == self.uaa_name:
                self.uaa = uaa

    def cf_push(self):
        server_dir = os.path.join(workdir(), 'server')
        with tools.chdir(server_dir):
            check_call([cf(), 'push', self.app_name])

    def cf_restage(self):
        puts(colored.cyan("Reconfiguring the application. Please wait a minute."))
        self.cf_push()

    def cf_env(self):
        url = '/v2/apps/%s/env' % self.app_guid
        return cf_curl(url)

    def create_timeseries_service(self):
        msg = colored.cyan("\nWe will now create the ")
        msg += colored.cyan("Time Series", bold=True)
        msg += colored.cyan(" service.")
        puts(msg)

        if not self.auto:
            msg = colored.yellow("The default answers for the following questions should be fine.")
            puts(msg)

        self.ts_name = self.auto_query("Service instance ID:", SETUP_DEFAULTS['ts_name'])
        plan = self.auto_query("Service plan:", SETUP_DEFAULTS['ts_plan'])

        # Creating the service
        params = {'trustedIssuerIds': [self.uaa['credentials']['issuerId']]}
        params = json.dumps(params)
        check_call([cf(), 'create-service', 'predix-timeseries', plan, self.ts_name, '-c', params])

        # Binding the service
        check_call([cf(), 'bind-service', self.app_name, self.ts_name])

        # get ts service data
        for ts in self.cf_env()['system_env_json']['VCAP_SERVICES']['predix-timeseries']:
            if ts['name'] == self.ts_name:
                self.ts = ts

    def auto_query(self, msg, default):
        return self.auto and default or prompt.query(msg, default)

    def predix_login(self):

        if not self.auto:
            self.username = prompt.query("Predix.io username:")
            self.password = getpass("Predix.io password: ")
            msg = "Select your Predix.io API region. If not sure, pick (1)."
            api_endpoint = prompt.options(msg, REGIONS, default='1')
        else:
            api_endpoint = REGIONS[0]['return']

        org = self.auto_query("Predix.io org (usually same as username):", self.username)
        space = self.auto_query("Predix.io space (usually 'dev'):", SETUP_DEFAULTS['space'])

        puts("")
        puts("Trying to login. Please wait a second.")
        check_call([cf(), 'api', api_endpoint])
        try:
            check_call([cf(), 'auth', self.username, self.password])
        except CalledProcessError:
            puts(colored.red('Unable to login. Please check the log output and '
                             'try again with a different user/password/region.'))
            sys.exit(1)

        try:
            check_call([cf(), 'target', '-o', org, '-s', space])
        except CalledProcessError:
            puts(
                colored.red('Unable to set your org/space. Please check the values and try again.'))
            check_call([cf(), 'orgs'])
            check_call([cf(), 'spaces'])
            sys.exit(1)


def basic_auth(username, password):
    return {
        'Authorization': 'Basic ' + base64.b64encode(username + ':' + password)
    }


def bearer_auth(token):
    return {
        'Authorization': 'Bearer ' + token
    }


def download_cloundfoundry():
    if platform.machine() not in ('x86_64', 'AMD64'):
        puts(colored.red('Error: only 64-bit machines are supported at the moment.'))
        sys.exit(1)

    if not os.path.exists('bin'):
        os.makedirs('bin')

    name = platform.system()

    url, target = CF_CLI.get(name, (None, None))
    if not url:
        puts(colored.red('Error: only Linux, MacOS and Windows are supported at the moment.'))
        sys.exit(1)

    if os.path.isfile(target):
        os.remove(target)
    tools.download(url, target)
    with tools.chdir('bin'):
        tools.unzip('../' + target)
    os.remove(target)


def cf():
    bin_dir = os.path.abspath(os.path.join(workdir(), 'bin'))
    return platform.system() == 'Windows' and os.path.join(bin_dir, 'cf.exe') \
           or os.path.join(bin_dir, 'cf')


def cf_curl(url, _json=True):
    v = check_output([cf(), 'curl', url])
    if _json:
        return json.loads(v)


def setup_cloundfoundry():
    """ Downloads the Cloundfoundry CLI app if needed """
    try:
        version = check_output([cf(), 'version'])
        puts('Cloundfoundry CLI version: ' + version)
    except:
        puts(colored.cyan('No Cloudfoundry CLI tool found. Downloading. Please wait.'))
        download_cloundfoundry()
        version = check_output([cf(), 'version'])
        puts('Cloundfoundry CLI version: ' + version)


def workdir():
    return os.path.dirname(os.path.dirname(THIS))


if __name__ == '__main__':
    os.chdir(workdir())
    setup_cloundfoundry()
    PredixSetup().run()
