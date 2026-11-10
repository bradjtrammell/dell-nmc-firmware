"""Mock HTTP tests for read-only H910P resource verification."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from threading import Thread
import unittest
from tools.probe_live_nmc import probe,verify_response

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        self.server.requests.append(('GET',self.path,self.headers.get('Authorization')))
        body=self.server.assets.get(self.path)
        if body is None:self.send_error(404);return
        self.send_response(200)
        self.send_header('Content-Type','application/javascript')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers();self.wfile.write(body)
    def do_POST(self):
        self.server.requests.append(('POST',self.path,None))
        self.send_error(405)

class LiveProbeTests(unittest.TestCase):
    def test_no_body_contents_in_failure_diagnostics(self):
        result=verify_response(b'secretA',b'secretB')
        self.assertFalse(result['match'])
        self.assertEqual(result['first_differing_byte'],6)
        self.assertNotIn('secret',str(result))
    def test_all_get_and_binary_integrity(self):
        server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        server.assets={'/index.htm':b'<main>hello</main>','/Help/resource.htm':b'window.A="hi";'}
        server.requests=[]
        thread=Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            result=probe(f'http://127.0.0.1:{server.server_port}','admin','secret',list(server.assets),server.assets,10)
            self.assertTrue(result['all_exact'])
            self.assertEqual(len(server.requests),2)
            self.assertTrue(all(item[0]=='GET' for item in server.requests))
            self.assertTrue(all(item[2]=='Basic YWRtaW46c2VjcmV0' for item in server.requests))
            self.assertNotIn('secret',str(result))
        finally:server.shutdown();server.server_close();thread.join()

if __name__=='__main__':unittest.main()
