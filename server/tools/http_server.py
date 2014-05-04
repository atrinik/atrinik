#!/usr/bin/python

import os, sys

# py3k
try:
    from socketserver import TCPServer
    from http.server import SimpleHTTPRequestHandler
    import configparser
    from urllib.parse import urlparse

    config = configparser.ConfigParser(strict = False)
except:
    from BaseHTTPServer import HTTPServer as TCPServer
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    import ConfigParser as configparser
    from urlparse import urlparse

    config = configparser.ConfigParser()

config.readfp(open("server.cfg"))
config.read(["server-custom.cfg"])

class HTTPRequestHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

if __name__ == '__main__':
    if config.getboolean("general", "http_server"):
        os.chdir(config.get("general", "httppath"))
        o = urlparse(config.get("general", "http_url"))
        server = TCPServer(("", o.port), HTTPRequestHandler)
        server.serve_forever()
