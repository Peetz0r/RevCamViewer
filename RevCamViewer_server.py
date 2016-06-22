#!/usr/bin/python3

import sys, os, datetime, http.server, socket, socketserver, signal, requests
from PIL import Image
from io import BytesIO

def out(str):
	print(datetime.datetime.now().strftime('[%Y-%m-%d_%H:%M:%S.%f]'), str)

class MyWebServer(socketserver.ForkingMixIn, http.server.HTTPServer):
	def server_bind(self):
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		self.socket.bind(self.server_address)

class WebHandler(http.server.BaseHTTPRequestHandler):
	protocol_version = 'HTTP/1.1'
	def do_GET(self):
		out('['+self.path+'] ['+repr(self.client_address)+'] do_GET')
		if not self.path.startswith('/cam'):
			self.send_response(404)
			self.end_headers()
			self.wfile.write(b'not a cam')
			out('['+self.path+'] ['+repr(self.client_address)+'] not a cam')
		else:
			cam = self.path[4:]
			url = 'http://pov/cgi-bin/nph-mjgrab?'+cam
			out('[camera '+cam+'] ['+url+']')

			attempts = 4
			while attempts > 0:
				out('[camera '+cam+'] [Attempt '+ str(4-attempts)+']')
				try:
					attempts -= 1
					if(attempts == 0):
						im_io = BytesIO(requests.get('http://cnyweather.com/images/offline.jpg').content)
					else:
						im_io = BytesIO(requests.get(url).content)
					im = Image.open(im_io)
					im = im.resize((160, 128), Image.BICUBIC)
					io_tmp = BytesIO()
					im.save(io_tmp, 'jpeg', quality=80)
					self.send_response(200)
					length = io_tmp.seek(0,2)
					self.send_header('Content-Length', length)
					self.end_headers()
					io_tmp.seek(0)
					self.wfile.write(io_tmp.read())
					attempts = 0
					out('[camera '+cam+'] [Done!] Length: '+str(length)+']')
				except Exception as e:
					out(repr(e))
		self.finish()
		self.connection.close()

def signal_handler(signal, frame):
	out('SIGINT received, shutting down')
	sys.exit(0)

if __name__ == "__main__":
	out('begin')
	server = MyWebServer(('', 9042), WebHandler)
	out('started httpserver')
	try:
		server.serve_forever()
	except KeyboardInterrupt:
		pass
	server.server_close()
	out('end')


