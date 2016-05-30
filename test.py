import socket

def send(request):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8080))
    s.sendall(request)
    r = recv_all(s)
    s.close()
    return r
    
def recv_all(s, chunk_size=1024):
    data = ''
    while True:
        d = s.recv(chunk_size)
        if not d:
            break
        data += d
    return data

def send_http(url):
    return send('GET ' + url + ' HTTP/1.0\r\n')
