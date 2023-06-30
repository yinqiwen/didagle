# print("my_package is imported")
import os
import sys
import json
import argparse
import requests

from .didagle import (
    new_block,
    new_if,
    new_else,
    new_while,
)


parser = argparse.ArgumentParser(description='Didagle Python Script')
parser.add_argument('-e', '--evaluate', action='store_true')  # on/off flag
parser.add_argument(
    '-s', '--server', help='remote evaluate http server address')
parser.add_argument('--args', help='evaluate json args string')
parser.add_argument('--json_output', action='store_true',
                    help='format evaluate output as json string')
parser.add_argument('--meta_file', help='didagle json meta file path')
parser.add_argument('--png', action='store_true',
                    help='dump png file in non-evaluate mode')
parser.add_argument('--toml', action='store_true',
                    help='dump toml file in non-evaluate mode')
didagle.run_args = parser.parse_args()
if didagle.run_args.evaluate:
    if didagle.run_args.server is None:
        print("Missing evaluate server '--server' to evaluate.", file=sys.stderr)
        parser.print_help(file=sys.stderr)
        sys.exit(-1)
    if didagle.run_args.args is not None:
        try:
            json.loads(didagle.run_args.args)
        except ValueError:  # includes simplejson.decoder.JSONDecodeError
            print('args Must be valid json string.', file=sys.stderr,)
            parser.print_help(file=sys.stderr)
            sys.exit(-1)
    url = "http://" + didagle.run_args.server + "/appengine/get_meta_json"
    r = requests.post(url)
    if r.status_code != 200:
        print(
            "Invalid 'get_meta_json' response code:" + str(r.status_code),
            file=sys.stderr,
        )
        sys.exit(-1)
    didagle.load_operator_meta_json_content(r.json()["json"])
else:
    if didagle.run_args.meta_file is None:
        print("Missing args '--meta_file'", file=sys.stderr)
        parser.print_help(file=sys.stderr)
        sys.exit(-1)
    didagle.load_operator_meta_json(didagle.run_args.meta_file)
    if not didagle.run_args.png and not didagle.run_args.toml:
        print("Nothinf to do since no --png or --toml specified.", file=sys.stderr)
        parser.print_help(file=sys.stderr)
        sys.exit(-1)
