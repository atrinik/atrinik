#!/usr/bin/python3

import os, sys, hashlib, platform, io

# py3k
try:
    from http.server import HTTPServer, SimpleHTTPRequestHandler
    import configparser
    from urllib.parse import urlparse
    from urllib.parse import unquote, quote
    import socketserver
    from html import escape

    config = configparser.ConfigParser(strict = False)
except:
    from BaseHTTPServer import HTTPServer
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    import ConfigParser as configparser
    from urlparse import urlparse
    from urllib import unquote, quote
    import SocketServer as socketserver
    from cgi import escape

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

    def list_directory(self, path):
        try:
            l = os.listdir(path)
        except os.error:
            self.send_error(404, "No permission to list directory")
            return None

        l = [entry for entry in l if not entry.startswith(".")]
        l.sort(key=lambda a: a.lower())
        r = []
        displaypath = escape(unquote(self.path))
        enc = sys.getfilesystemencoding()
        title = 'Directory listing for %s' % displaypath
        r.append('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" '
                 '"http://www.w3.org/TR/html4/strict.dtd">')
        r.append('<html>\n<head>')
        r.append('<meta http-equiv="Content-Type" '
                 'content="text/html; charset=%s">' % enc)
        r.append('<title>%s</title>\n</head>' % title)
        r.append('<body>\n<h1>%s</h1>' % title)
        r.append('<hr>\n<ul>')
        for name in l:
            fullname = os.path.join(path, name)
            displayname = linkname = name
            if os.path.isdir(fullname):
                displayname = name + "/"
                linkname = name + "/"
            if os.path.islink(fullname):
                displayname = name + "@"
            r.append('<li><a href="%s">%s</a></li>'
                    % (quote(linkname), escape(displayname)))
        r.append('</ul>\n<hr>\n</body>\n</html>\n')
        encoded = '\n'.join(r).encode(enc)
        f = io.BytesIO()
        f.write(encoded)
        f.seek(0)
        self.send_response(200)
        self.send_header("Content-type", "text/html; charset=%s" % enc)
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        return f

    def send_head(self):
        path = self.translate_path(self.path)
        f = None

        if os.path.basename(path).startswith("."):
            self.send_error(403, "Access denied")
            return None

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
        if isinstance(source, io.BytesIO):
            return SimpleHTTPRequestHandler.copyfile(self, source, outputfile)

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
