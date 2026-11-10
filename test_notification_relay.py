#!/usr/bin/env python3
"""Local-only relay integration tests; outbound notifications are mocked."""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('h910p_notification_relay', HERE/'notification-relay'/'relay.py')
relay = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(relay)

class FakeMonitor:
    last_success = 0
    online = True

class RelayIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.store = relay.Store(Path(self.temp.name)/'destinations.json')
        self.server = relay.Server(('127.0.0.1',0), {
            'admin_token':'test-token-32-chars-long-000000001',
            'allowed_origin':'http://192.168.0.174',
            'allow_http_destinations':False
        },self.store,FakeMonitor())
        self.thread = threading.Thread(target=self.server.serve_forever,daemon=True)
        self.thread.start()
        self.base='http://127.0.0.1:'+str(self.server.server_port)

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(timeout=3)
        self.temp.cleanup()

    def request(self,method,path,payload=None,authorized=True,origin='http://192.168.0.174'):
        headers={'Origin':origin}
        if authorized:headers['Authorization']='Bearer test-token-32-chars-long-000000001'
        data=None if payload is None else json.dumps(payload).encode()
        if data is not None:headers['Content-Type']='application/json'
        request=Request(self.base+path,data=data,headers=headers,method=method)
        try:
            with urlopen(request,timeout=3) as result:
                return result.status,json.loads(result.read() or '{}'),dict(result.headers)
        except HTTPError as e:
            return e.code,json.loads(e.read() or '{}'),dict(e.headers)

    def test_authentication_and_cors_reject_bad_access(self):
        status,_,_=self.request('GET','/api/v1/destinations',authorized=False)
        self.assertEqual(status,401)
        status,_,_=self.request('GET','/api/v1/destinations',origin='https://unknown.example')
        self.assertEqual(status,403)
        status,data,headers=self.request('GET','/api/v1/destinations')
        self.assertEqual(status,200)
        self.assertEqual(data['destinations'],[])
        self.assertEqual(headers['Access-Control-Allow-Origin'],'http://192.168.0.174')

    def test_unlimited_relative_to_four_email_slots_and_secret_redaction(self):
        for i in range(6):
            kind=['monita','ntfy','discord'][i%3]
            if kind=='monita':payload={'type':kind,'name':f'Monita {i}','url':'https://monita.example','token':'app-secret','enabled':True}
            elif kind=='ntfy':payload={'type':kind,'name':f'ntfy {i}','url':'https://ntfy.example','topic':f'h910p_{i}','token':'ntfy-secret','enabled':True}
            else:payload={'type':kind,'name':f'Discord {i}','url':'https://discord.com/api/webhooks/123456789/abcDEFxyz','enabled':True}
            status,record,_=self.request('POST','/api/v1/destinations',payload)
            self.assertEqual(status,201,record)
            self.assertNotIn('token',record)
            if kind=='discord':self.assertNotIn('url',record)
        status,result,_=self.request('GET','/api/v1/destinations')
        self.assertEqual(status,200)
        self.assertEqual(len(result['destinations']),6)
        self.assertNotIn('app-secret',json.dumps(result))
        self.assertNotIn('ntfy-secret',json.dumps(result))
        self.assertNotIn('abcDEFxyz',json.dumps(result))
        reopened=relay.Store(Path(self.temp.name)/'destinations.json')
        self.assertEqual(len(reopened.snapshot()),6)
        self.assertEqual((Path(self.temp.name)/'destinations.json').stat().st_mode&0o777,0o600)

    def test_update_retains_credential_and_test_propagates_delivery_failure(self):
        payload={'type':'monita','name':'Main','url':'https://monita.example','token':'secret','enabled':True}
        status,record,_=self.request('POST','/api/v1/destinations',payload)
        self.assertEqual(status,201)
        key=record['id']
        status,updated,_=self.request('PUT','/api/v1/destinations/'+key,{'type':'monita','name':'Renamed','url':'https://monita.example','enabled':False})
        self.assertEqual(status,200)
        self.assertFalse(updated['enabled'])
        self.assertEqual(self.store.get(key)['token'],'secret')
        with patch.object(relay,'send',return_value=202) as mock_send:
            status,result,_=self.request('POST','/api/v1/destinations/'+key+'/test',{})
            self.assertEqual(status,200)
            self.assertTrue(result['sent'])
            mock_send.assert_called_once()
        with patch.object(relay,'send',side_effect=RuntimeError('fake downstream failure')):
            status,result,_=self.request('POST','/api/v1/destinations/'+key+'/test',{})
            self.assertEqual(status,502)
            self.assertNotIn('secret',json.dumps(result))
        status,_,_=self.request('DELETE','/api/v1/destinations/'+key)
        self.assertEqual(status,200)
        status,result,_=self.request('GET','/api/v1/destinations')
        self.assertEqual(result['destinations'],[])

    def test_no_plaintext_destination_urls_by_default(self):
        for kind,payload in [
          ('monita',{'type':'monita','name':'M','url':'http://example.com','token':'t'}),
          ('ntfy',{'type':'ntfy','name':'N','url':'http://example.com','topic':'ups'}),
        ]:
            status,_,_=self.request('POST','/api/v1/destinations',payload)
            self.assertEqual(status,400)

if __name__=='__main__':unittest.main(verbosity=2)
