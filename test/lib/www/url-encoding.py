# -*- encoding: utf-8 -*-
import time
import io
import urllib.parse


def html_esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def do_response(req, body, code=200, headers=None):
    if headers is None:
        headers = {}
    if isinstance(body, str):
        body_bytes = body.encode('utf-8')
    else:
        body_bytes = body
    req.send_response(code)
    req.send_header('Content-Length', str(len(body_bytes)))
    if 'Content-Type' not in headers:
        req.send_header('Content-Type', 'text/html; charset=utf-8')
    for k, v in headers.items():
        if k != 'Content-Length':
            req.send_header(k, v)
    req.end_headers()
    req.wfile.write(body.encode("utf-8"))
    return io.StringIO()  # dummy return for compatibility


def do_redirect(req, target):
    return do_response(
        req,
        '<!doctype html><a href="{}">Go here</a>'.format(html_esc(target)),
        code=302,
        headers={'Location': target}
    )


def handle_request(req):
    url = urllib.parse.urlparse(req.path)
    query = url.query

    if query == '/':
        return do_redirect(req, '?/%83y%81[%83W')  # ページ

    elif query == '/f':
        return do_response(req,
            '<!doctype html public "-//W3C//DTD HTML 4.01 Frameset//EN" '
            '"http://www.w3.org/TR/html4/frameset.dtd">'
            '<html><head><title>framed</title></head>'
            '<frameset cols="50%,50%">'
            '<frame src="?/%98g" name="a">'
            '<frame src="?/%95s%96%D1%82%C8%98_%91%88" name="b">'
            '</frameset>')

    elif query == '/r':
        return do_response(req,
            '<!doctype html><script src="?/%8F%91"></script>')

    elif query == '/re':
        return do_response(req,
            '<!doctype html>'
            '<img src="?/%8C%CC%8F%E1">'
            '<img src="?/%89i%8Bv">')

    elif query == '/%83y%81[%83W':  # ページ
        return do_response(req, '<!doctype html><h1>PASS</h1>')

    elif query == '/%98g':  # 枠
        return do_response(req, '<!doctype html><h1>PASS</h1>')

    elif query == '/%95s%96%D1%82%C8%98_%91%88':  # 不毛な論争
        return do_response(req, '<!doctype html><h1>FRAME</h1>')

    elif query == '/%8F%91':  # 書
        return do_response(req,
            'window.onload=function(){'
            'document.body.innerHTML="<h1>PASS</h1>";};',
            headers={'Content-Type': 'application/javascript'})

    elif query == '/%8C%CC%8F%E1':  # 故障
        return do_response(req,
            '<!doctype html>internal server error',
            code=500)

    elif query == '/%89i%8Bv':  # 永久
        time.sleep(5)
        return do_response(req, '', code=204)

    else:
        return do_response(req,
            '<!doctype html><title>404 Not Found</title>'
            '<p>URL not found: {}</p>'.format(html_esc(req.path)),
            code=404)
