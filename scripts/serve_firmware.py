#!/usr/bin/env python3
"""
Simple HTTP server that serves a firmware file named `firmware.bin` from the
current directory at path /firmware.bin. Run this on a machine reachable by
the ESP32.
"""
import http.server
import socketserver
import os

PORT = 8000

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(b'Place firmware.bin in this folder and GET /firmware.bin')
            return
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

if __name__ == '__main__':
    cwd = os.getcwd()
    print(f"Serving {cwd} on port {PORT}. Make sure firmware.bin exists here.")
    with socketserver.TCPServer(('', PORT), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print('\nStopped')
