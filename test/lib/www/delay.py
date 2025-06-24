from io import BytesIO
import urllib.parse as urlparse
import time

def handle_request(req):
    url = urlparse.urlparse(req.path)
    try:
        delay = float(int(url.query))
    except ValueError:
        delay = 0

    time.sleep(delay / 1000)  # argument is in milliseconds

    body = "OK ({}ms delayed)\n".format(int(delay))
    body_bytes = body.encode('utf-8')

    req.send_response(200)
    req.send_header('Content-Type', 'text/plain')
    req.send_header('Content-Length', str(len(body_bytes)))
    req.end_headers()
    return BytesIO(body_bytes)
