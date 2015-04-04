#!/usr/bin/python

import os, sys, hashlib, platform

# py3k
try:
    from http.server import HTTPServer, SimpleHTTPRequestHandler
    import configparser
    from urllib.parse import urlparse
    import socketserver

    config = configparser.ConfigParser(strict = False)
except:
    from BaseHTTPServer import HTTPServer
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    import ConfigParser as configparser
    from urlparse import urlparse
    import SocketServer as socketserver

    config = configparser.ConfigParser()

if any(platform.win32_ver()):
    SocketServerMixIn = socketserver.ThreadingMixIn
else:
    SocketServerMixIn = socketserver.ForkingMixIn

config.readfp(open("server.cfg"))
config.read(["server-custom.cfg"])

class HTTPRequestHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def send_head(self):
        path = self.translate_path(self.path)
        f = None

        if os.path.isdir(path):
            if not self.path.endswith('/'):
                self.send_response(301)
                self.send_header("Location", self.path + "/")
                self.end_headers()
                return None

            for index in "index.html", "index.htm":
                index = os.path.join(path, index)
                if os.path.exists(index):
                    path = index
                    break
            else:
                return self.list_directory(path)

        ctype = self.guess_type(path)

        try:
            f = open(path, 'rb')
        except IOError:
            self.send_error(404, "File not found")
            return None

        if_none_match = self.headers.get("If-None-Match")
        sha1 = hashlib.sha1(f.read()).hexdigest()

        if if_none_match == sha1:
            self.send_response(304)
            self.end_headers()
            f.close()
            return None

        f.seek(0)

        try:
            self.send_response(200)
            self.send_header("Content-type", ctype)
            fs = os.fstat(f.fileno())
            self.send_header("Content-Length", str(fs[6]))
            self.send_header("Last-Modified", self.date_time_string(fs.st_mtime))
            self.send_header("ETag", sha1)
            self.end_headers()
            return f
        except:
            f.close()
            raise

class ForkingHTTPServer(SocketServerMixIn, HTTPServer):
    def finish_request(self, request, client_address):
        request.settimeout(60)
        HTTPServer.finish_request(self, request, client_address)

if __name__ == '__main__':
    if config.getboolean("general", "http_server"):
        os.chdir(config.get("general", "httppath"))
        o = urlparse(config.get("general", "http_url"))
        server = ForkingHTTPServer(("", o.port), HTTPRequestHandler)
        server.serve_forever()
