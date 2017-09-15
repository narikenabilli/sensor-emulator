#
# Copyright 2017, TopCoder Inc. All rights reserved.
#

"""
This server exists only to create a new application using CF CLI.
"""
from __future__ import unicode_literals, print_function
import os
import requests
from requests.auth import HTTPBasicAuth
from ConfigParser import ConfigParser
from flask import Flask, render_template, request, make_response
import datetime

# setup Flask
app = Flask(__name__)
port = int(os.getenv("PORT", "5000"))

# setup cfg
cfg = ConfigParser()
if cfg.read('server.ini'):
    configured = True
else:
    configured = False


@app.route('/')
def report():
    if not configured:
        return 'Not configured. Please restart server with the INI configuration file present.'

    ctx = dict()
    ctx['access_token'] = request.cookies.get('access_token')
    if not ctx['access_token']:
        fetch_access_token(ctx)

    tags = fetch_tags(ctx)
    ts_data = []
    for tag in tags:
        for item in fetch_ts_data(ctx, tag):
            ts_data.append(item)

    asset_data = fetch_asset_data(ctx)

    resp = make_response(
        render_template("report.html", ts_data=ts_data, asset_data=asset_data))
    resp.set_cookie('access_token', ctx['access_token'])
    return resp


@app.template_filter('to_date')
def to_date(timestamp):
    dt = datetime.datetime.fromtimestamp(timestamp / 1000.0)
    return dt.strftime('%Y-%m-%d %H:%M:%S')


def fetch_asset_data(ctx):
    base = cfg.get('asset', 'uri')
    url = base + cfg.get('asset', 'collection') + '?pageSize=1000'
    zone_id = cfg.get('asset', 'zone_id')

    headers = {
        'Predix-Zone-Id': zone_id
    }
    r = do_auth_request(ctx, 'GET', url, headers=headers)
    if r.ok:
        return r.json()
    else:
        raise Exception("[%s] %s" % (r.status_code, r.reason))


def fetch_ts_data(ctx, tag):
    url = cfg.get('timeseries', 'query_uri')
    zone_id = cfg.get('timeseries', 'zone_id')
    body = {
        "tags": [
            {
                "name": tag,
                "order": "desc",
                "limit": 100
            }
        ],
        "start": "2y-ago"
    }
    headers = {
        'Predix-Zone-Id': zone_id
    }
    r = do_auth_request(ctx, 'POST', url, json=body, headers=headers)
    if r.ok:
        items = [dict(tag=tag, timestamp=x[0], val=x[1], quality=x[2])
                 for x in r.json()['tags'][0]['results'][0]['values']]
        return items
    else:
        raise Exception("[%s] %s" % (r.status_code, r.reason))


def do_auth_request(ctx, *args, **kwargs):
    headers = kwargs.setdefault('headers', dict())
    headers['Authorization'] = 'Bearer ' + ctx['access_token']

    while True:
        r = requests.request(*args, **kwargs)
        if r.status_code == 401:
            fetch_access_token(ctx)
        else:
            return r


def fetch_tags(ctx):
    base = cfg.get('timeseries', 'query_uri')  # type: str
    url = base.replace('/datapoints', '/tags')
    zone_id = cfg.get('timeseries', 'zone_id')
    headers = {
        'Predix-Zone-Id': zone_id
    }
    r = do_auth_request(ctx, 'GET', url, headers=headers)
    if r.ok:
        return r.json()['results']
    else:
        raise Exception("[%s] %s" % (r.status_code, r.reason))


def fetch_access_token(ctx):
    client_id = cfg.get('server', 'client_id')
    client_secret = cfg.get('server', 'client_secret')
    ctx['access_token'] = uaa_login(client_id, client_secret)


def uaa_login(client_id, client_secret):
    base = cfg.get('uaa', 'uri')
    url = base + '/oauth/token'
    params = {
        'response_type': 'token',
        'grant_type': 'client_credentials'
    }
    r = requests.post(url, data=params, auth=HTTPBasicAuth(client_id, client_secret), timeout=30)
    if not r.ok:
        raise Exception('Login error')
    data = r.json()
    return data['access_token']


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=port)
