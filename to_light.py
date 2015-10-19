#!/usr/bin/python

import os, sys
import socket
import requests

#sock = socket.socket()
#sock.connect(("10.91.114.211", 8082))
#sock.send( "POST /in_dragon/1.webm HTTP/1.1\r\n"
#             "User-Agent: python-piper\r\n"
#             "Host: 10.91.144.211:8082\r\n"
#             "Connection: close\r\n"
#             "Transfer-Encoding: chunked\r\n"
#             "Icy-MetaData: 1\r\n\r\n")

#with open ('/tmp/stream', 'r') as f:
with open ('/tmp/test.webm', 'r') as f:
	requests.post("http://127.0.0.1:8082/in_dragon/1.webm", data=f)


#while (True):
#	data = webm_pipe.read(1444)
#	sock.sendall(data)
