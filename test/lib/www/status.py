# -*- coding: utf-8 -*-
import io
import urllib.parse


def html_esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def handle_request(req):
    url = urllib.parse.urlparse(req.path)
    headers = []
    body = ""

    try:
        query = urllib.parse.parse_qsl(url.query, strict_parsing=True)
        status = None
        for key, value in query:
            if key == 'status':
                if status is not None:
                    raise ValueError("status can only be specified once")
                status = int(value)
            elif key in ('Content-Type', 'Content-Length'):
                raise ValueError(f"cannot override {key}")
            else:
                headers.append((key, value))

        if status is None:
            status = 200

        body = "<!doctype html><h1>Status: {}</h1>".format(status)
        if headers:
            body += "<pre>"
            for key, value in headers:
                body += html_esc(f"{key}: {value}\n")
            body += "</pre>"

    except Exception as e:
        try:
            status = int(url.query)
            body = "<!doctype html><h1>Status: {}</h1>".format(status)
        except:
            status = 400
            body = "<!doctype html><h1>Status: 400</h1>"
            body += "<pre>" + html_esc(str(e)) + "</pre>"

    body_bytes = body.encode('utf-8')
    req.send_response(status)
    req.send_header('Content-Type', 'text/html; charset=utf-8')
    req.send_header('Content-Length', str(len(body_bytes)))
    for key, value in headers:
        req.send_header(key, value)
    req.end_headers()
    req.wfile.write(body_bytes)
    return io.StringIO()  # dummy return
