#!/usr/bin/python3

import os, sys, hashlib, platform

# py3k
try:
    from http.server import HTTPServer, SimpleHTTPRequestHandler
    import configparser
    from urllib.parse import urlparse
    from urllib.parse import unquote
    import socketserver

    config = configparser.ConfigParser(strict = False)
except:
    from BaseHTTPServer import HTTPServer
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    import ConfigParser as configparser
    from urlparse import urlparse
    from urllib import unquote
    import SocketServer as socketserver

    config = configparser.ConfigParser()

if any(platform.win32_ver()):
    SocketServerMixIn = socketserver.ThreadingMixIn
else:
    SocketServerMixIn = socketserver.ForkingMixIn

config.readfp(open("server.cfg"))
config.read(["server-custom.cfg"])

path_translations = {
    "resources": config.get("general", "resourcespath"),
}
path_default = config.get("general", "httppath")

class HTTPRequestHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def translate_path(self, path):
        if not path.startswith("/"):
            raise ValueError("invalid path")

        path = unquote(path)
        path = path[1:]
        prefix_end = path.find("/")
        prefix = path if prefix_end == -1 else path[:prefix_end]

        translated = path_translations.get(prefix)
        if translated is not None:
            return translated + "/" + path[len(prefix):]

        return path_default + "/" + path

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

    def copyfile(self, source, outputfile):
        block_size = 16 * 1024
        while True:
            buf = source.read(block_size)
            if not buf:
                break

            total = len(buf)
            pos = 0
            while pos < total:
                pos += outputfile.write(buf[pos:])

class ForkingHTTPServer(SocketServerMixIn, HTTPServer):
    def finish_request(self, request, client_address):
        request.settimeout(60)
        HTTPServer.finish_request(self, request, client_address)

if __name__ == '__main__':
    if config.getboolean("general", "http_server"):
        o = urlparse(config.get("general", "http_url"))
        server = ForkingHTTPServer(("", o.port), HTTPRequestHandler)
        server.serve_forever()
