import json
from urllib import parse as urlparse
from io import StringIO


def handle_request(req):
    url = urlparse.urlparse(req.path)
    headers = {}
    for name, value in req.headers.items():
        headers[name] = value.rstrip()

    d = dict(
        command=req.command,
        version=req.protocol_version,
        origin=req.client_address,
        url=req.path,
        path=url.path,
        params=url.params,
        query=url.query,
        fragment=url.fragment,
        headers=headers,
        postdata=req.postdata.decode('utf-8') if isinstance(req.postdata, bytes) else req.postdata,
    )

    body = json.dumps(d, indent=2) + '\n'

    req.send_response(200)
    req.send_header('Content-Type', 'application/json')
    req.send_header('Content-Length', str(len(body.encode('utf-8'))))
    req.end_headers()
    return StringIO(body)
