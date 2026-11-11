#!/usr/bin/env python3
"""Optional LAN notification relay for Dell H910P; Python 3.11+ stdlib.

The H910P has exactly four native email slots and no built-in outbound HTTP
webhook API. This opt-in service polls read-only UPS XML, detects transitions,
and delivers Monita, Discord, and ntfy events independently of the browser.
It never issues power or configuration writes to the NMC.
"""
from __future__ import annotations
import argparse
import base64
import hmac
import json
import logging
import os
from pathlib import Path
import re
import secrets
import ssl
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse, quote
from urllib.request import Request, build_opener, HTTPSHandler, HTTPHandler, HTTPRedirectHandler
from xml.etree import ElementTree

LOG=logging.getLogger('h910p-relay')
MAX_BODY=16384
EVENT_TYPES=('power_lost','power_restored','battery_low','battery_recovered','connection_lost','connection_restored')
class ValidationError(ValueError): pass

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValidationError('Redirects are not permitted for webhook delivery')

OPENER=build_opener(HTTPHandler(),HTTPSHandler(context=ssl.create_default_context()),NoRedirect())

def expect_url(value, *, allow_http=False):
    if not isinstance(value,str) or len(value)>700: raise ValidationError('Invalid URL')
    parsed=urlparse(value.strip())
    if parsed.scheme!='https' and not (allow_http and parsed.scheme=='http'):
        raise ValidationError('Destination must use HTTPS')
    if not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ValidationError('Destination URL may not have embedded credentials, query or fragment')
    if parsed.port is not None and not 1<=parsed.port<=65535: raise ValidationError('Invalid destination port')
    return value.strip().rstrip('/')

def destination_spec(payload, allow_http=False):
    if not isinstance(payload,dict): raise ValidationError('Expected a destination object')
    kind=payload.get('type')
    if kind not in ('monita','discord','ntfy'): raise ValidationError('Unknown destination type')
    name=payload.get('name')
    if not isinstance(name,str) or not 1<=len(name.strip())<=80: raise ValidationError('Name must be 1-80 characters')
    events=payload.get('events',list(EVENT_TYPES))
    if not isinstance(events,list) or len(events)>len(EVENT_TYPES) or len(events)!=len(set(map(str,events))) or any(e not in EVENT_TYPES for e in events):
        raise ValidationError('Invalid event subscriptions')
    value={'type':kind,'name':name.strip(),'enabled':payload.get('enabled',True) is True,'events':events}
    if kind=='monita':
        value['url']=expect_url(payload.get('url'),allow_http=allow_http)
        token=payload.get('token')
        if not isinstance(token,str) or not 1<=len(token)<=512: raise ValidationError('Monita application token is required')
        value['token']=token
    elif kind=='discord':
        # Discord's webhook URL embeds its token; never log or expose it.
        url=payload.get('url')
        if not isinstance(url,str) or not re.fullmatch(r'https://(?:discord\.com|discordapp\.com)/api/webhooks/[0-9]+/[A-Za-z0-9_.-]+',url):
            raise ValidationError('Use a valid Discord HTTPS webhook URL')
        value['url']=url
    else:
        value['url']=expect_url(payload.get('url'),allow_http=allow_http)
        topic=payload.get('topic')
        if not isinstance(topic,str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,120}',topic):raise ValidationError('Invalid ntfy topic')
        token=payload.get('token','')
        if not isinstance(token,str) or len(token)>512:raise ValidationError('Invalid ntfy token')
        value['topic']=topic;value['token']=token
    return value

def public_destination(d):
    out={k:d[k] for k in ('id','name','type','enabled') if k in d}
    if d['type']=='discord':out['configured']=True  # never return token-containing webhook URL
    else:
        out['url']=d.get('url','')
        if d['type']=='ntfy':out['topic']=d.get('topic','')
    out['has_token']=bool(d.get('token')) or d['type']=='discord'
    out['events']=d.get('events',list(EVENT_TYPES))
    return out

class Store:
    def __init__(self,path:Path):
        self.path=path;self.lock=threading.RLock();self.destinations=[]
        if path.exists():
            loaded=json.loads(path.read_text(encoding='utf-8'))
            if not isinstance(loaded,list):raise RuntimeError('Invalid destination store')
            self.destinations=loaded
    def snapshot(self):
        with self.lock:return [x.copy() for x in self.destinations]
    def write(self):
        self.path.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
        fd,temp=tempfile.mkstemp(prefix='.pending-',dir=str(self.path.parent))
        try:
            os.fchmod(fd,0o600)
            with os.fdopen(fd,'w',encoding='utf-8') as f:
                json.dump(self.destinations,f,separators=(',',':'))
                f.flush();os.fsync(f.fileno())
            os.replace(temp,self.path)
        finally:
            if os.path.exists(temp):os.unlink(temp)
    def create(self,spec):
        with self.lock:
            record={'id':secrets.token_hex(12),**spec}
            self.destinations.append(record);self.write()
            return public_destination(record)
    def get(self,key):
        with self.lock:
            record=next((x for x in self.destinations if x['id']==key),None)
            return record.copy() if record else None
    def update(self,key,spec):
        with self.lock:
            for i,existing in enumerate(self.destinations):
                if existing['id']==key:
                    if spec['type']!=existing['type']:raise ValidationError('Destination type cannot be changed')
                    self.destinations[i]={'id':key,**spec};self.write()
                    return public_destination(self.destinations[i])
            return None
    def delete(self,key):
        with self.lock:
            n=len(self.destinations)
            self.destinations=[x for x in self.destinations if x['id']!=key]
            if len(self.destinations)!=n:self.write();return True
            return False

def send(destination,title,message,allow_http=False):
    d=destination;kind=d['type']
    if kind=='monita':
        base=expect_url(d['url'],allow_http=allow_http)
        url=base+'/message';body=json.dumps({'title':title,'message':message,'priority':7}).encode()
        headers={'Content-Type':'application/json','X-Gotify-Key':d['token']}
    elif kind=='discord':
        url=d['url'];body=json.dumps({'content':title+'\n'+message}).encode()
        headers={'Content-Type':'application/json'}
    else:
        base=expect_url(d['url'],allow_http=allow_http)
        url=base+'/'+quote(d['topic'],safe='');body=message.encode()
        headers={'Content-Type':'text/plain; charset=utf-8','Title':title}
        if d.get('token'):headers['Authorization']='Bearer '+d['token']
    try:
        with OPENER.open(Request(url,data=body,headers=headers,method='POST'),timeout=10) as response:
            if not 200<=response.status<300:raise RuntimeError('Destination returned HTTP '+str(response.status))
            return response.status
    except HTTPError as e:raise RuntimeError('Destination returned HTTP '+str(e.code)) from None
    except URLError as e:raise RuntimeError('Unable to reach destination') from e

def read_nmc(base_url,auth_header=''):
    if not base_url.startswith(('http://','https://')):raise ValidationError('Invalid NMC URL')
    headers={'Content-Type':'application/x-www-form-urlencoded'}
    if auth_header:headers['Authorization']=auth_header
    req=Request(base_url.rstrip('/')+'/get_object.xml',b'objectName=UPS.*',headers,method='POST')
    with OPENER.open(req,timeout=10) as response:
        if response.status!=200:raise RuntimeError('NMC returned HTTP '+str(response.status))
        data=response.read(512000)
        if len(data)>=512000:raise ValueError('Unexpected NMC response size')
    root=ElementTree.fromstring(data)
    values={}
    for element in root.iter('OBJECT'):
        name=element.attrib.get('name')
        if name:values[name]=(element.attrib.get('value') or element.findtext('VALUE') or element.text or '').strip()
    if 'UPS.PowerSummary.PresentStatus.ACPresent' not in values:raise ValueError('UPS status missing')
    return values

class Monitor(threading.Thread):
    daemon=True
    def __init__(self,store,base_url,auth_header,interval,allow_http):
        super().__init__(name='UPS status monitor')
        self.store=store;self.base=base_url;self.auth=auth_header;self.interval=interval;self.allow_http=allow_http
        self.state=None;self.failures=0;self.online=True;self.stop_event=threading.Event()
        self.last_success=0;self.last_error=''
    def emit(self,event,title,message):
        for d in self.store.snapshot():
            if not d.get('enabled') or event not in d.get('events',EVENT_TYPES):continue
            try:send(d,title,message,self.allow_http)
            except Exception as exc:LOG.error('Delivery to %s (%s) failed: %s',d.get('name'),d.get('type'),type(exc).__name__)
    def update(self,values):
        power=values.get('UPS.PowerSummary.PresentStatus.ACPresent')
        if power not in ('0','1'):
            raise ValueError('Invalid NMC utility-power state')
        charge=values.get('UPS.PowerSummary.RemainingCapacity','')
        old=self.state
        # Require a valid charge percentage. Missing sensors are not low-battery alarms.
        valid=charge.isdecimal() and 0<=int(charge)<=100
        low=old['battery_low'] if old is not None else False
        if valid:
            if int(charge)<=20:low=True
            elif int(charge)>=25:low=False
        status={'power':power,'battery_low':low}
        self.state=status
        if old is None:return
        if power!=old['power']:
            event='power_restored' if power=='1' else 'power_lost'
            self.emit(event,'UPS utility power restored' if power=='1' else 'UPS running on battery','H910P reported a utility power state change.')
        if low and not old['battery_low']:
            self.emit('battery_low','UPS low battery','H910P battery charge fell to '+charge+'%.')
        if not low and old['battery_low']:
            self.emit('battery_recovered','UPS battery recovered','H910P battery charge rose to '+charge+'%.')
    def run(self):
        while not self.stop_event.is_set():
            try:
                values=read_nmc(self.base,self.auth)
                if not self.online:self.emit('connection_restored','UPS connection restored','H910P is responding again.')
                self.online=True;self.failures=0;self.last_error='';self.last_success=time.time()
                self.update(values)
            except Exception as exc:
                self.failures+=1;self.last_error=type(exc).__name__
                if self.failures==3 and self.online:
                    self.online=False;self.emit('connection_lost','UPS connection lost','H910P did not respond to three consecutive polls.')
                LOG.warning('NMC poll failed: %s',type(exc).__name__)
            self.stop_event.wait(self.interval)

class Server(ThreadingHTTPServer):
    daemon_threads=True
    def __init__(self,address,config,store,monitor):
        super().__init__(address,Handler)
        self.config=config;self.store=store;self.monitor=monitor

class Handler(BaseHTTPRequestHandler):
    server:Server
    def log_message(self,fmt,*args):
        LOG.info('%s %s',self.client_address[0],fmt%args)
    def send_json(self,status,data):
        raw=json.dumps(data,separators=(',',':')).encode()
        self.send_response(status);self.send_header('Content-Type','application/json; charset=utf-8')
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
        origin=self.headers.get('Origin','')
        if origin and origin==self.server.config['allowed_origin']:
            self.send_header('Access-Control-Allow-Origin',origin)
            self.send_header('Vary','Origin');self.send_header('Access-Control-Allow-Headers','Authorization, Content-Type')
            self.send_header('Access-Control-Allow-Methods','GET,POST,PUT,DELETE,OPTIONS')
        self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
    def authorized(self):
        expected=self.server.config['admin_token']
        candidate=self.headers.get('Authorization','')
        return hmac.compare_digest(candidate,'Bearer '+expected)
    def request_allowed(self):
        origin=self.headers.get('Origin','')
        return not origin or origin==self.server.config['allowed_origin']
    def do_OPTIONS(self):
        if not self.request_allowed():return self.send_json(403,{'error':'Origin not allowed'})
        return self.send_json(204,{})
    def get_body(self):
        try:n=int(self.headers.get('Content-Length','0'))
        except ValueError:raise ValidationError('Invalid Content-Length')
        if n<1 or n>MAX_BODY:raise ValidationError('Invalid request size')
        value=json.loads(self.rfile.read(n))
        if not isinstance(value,dict):raise ValidationError('Expected an object')
        return value
    def handle_api(self):
        if not self.request_allowed():return self.send_json(403,{'error':'Origin not allowed'})
        if not self.authorized():return self.send_json(401,{'error':'Unauthorized'})
        path=self.path.split('?',1)[0]
        base='/api/v1/destinations'
        parts=path[len(base):].strip('/').split('/') if path.startswith(base) else []
        try:
            if self.command=='GET' and path=='/api/v1/status':
                return self.send_json(200,{'connected':True,'last_success':self.server.monitor.last_success,'nmc_online':self.server.monitor.online})
            if path==base:
                if self.command=='GET':return self.send_json(200,{'destinations':[public_destination(x) for x in self.server.store.snapshot()]})
                if self.command=='POST':
                    spec=destination_spec(self.get_body(),self.server.config['allow_http_destinations'])
                    return self.send_json(201,self.server.store.create(spec))
            if len(parts)>=1 and re.fullmatch('[a-f0-9]{24}',parts[0] or ''):
                key=parts[0]
                if len(parts)==1 and self.command=='DELETE':
                    return self.send_json(200 if self.server.store.delete(key) else 404,{'deleted':True})
                if len(parts)==1 and self.command=='PUT':
                    current=self.server.store.get(key)
                    if not current:return self.send_json(404,{'error':'Not found'})
                    payload=self.get_body()
                    if 'token' not in payload and current['type'] in ('monita','ntfy'):payload['token']=current.get('token','')
                    if 'url' not in payload and current['type']=='discord':payload['url']=current['url']
                    spec=destination_spec(payload,self.server.config['allow_http_destinations'])
                    return self.send_json(200,self.server.store.update(key,spec))
                if len(parts)==2 and parts[1]=='test' and self.command=='POST':
                    destination=self.server.store.get(key)
                    if not destination:return self.send_json(404,{'error':'Not found'})
                    status=send(destination,'H910P test notification','Test sent from the H910P notification manager.',self.server.config['allow_http_destinations'])
                    return self.send_json(200,{'sent':True,'status':status})
            return self.send_json(404,{'error':'Not found'})
        except (ValidationError,ValueError,KeyError,json.JSONDecodeError) as exc:
            return self.send_json(400,{'error':str(exc)[:200]})
        except Exception as exc:
            LOG.exception('Request failed')
            return self.send_json(502,{'error':'Delivery or storage operation failed'})
    def do_GET(self):self.handle_api()
    def do_POST(self):self.handle_api()
    def do_PUT(self):self.handle_api()
    def do_DELETE(self):self.handle_api()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',required=True,type=Path)
    args=parser.parse_args()
    conf=json.loads(args.config.read_text(encoding='utf-8'))
    admin_token=os.environ.get('H910P_RELAY_ADMIN_TOKEN','')
    if len(admin_token)<32:parser.error('Set a 32+ character H910P_RELAY_ADMIN_TOKEN')
    allowed_origin=conf['allowed_origin']
    if not re.fullmatch(r'https?://[^/?#]+',allowed_origin):parser.error('Invalid allowed_origin')
    listen=conf.get('listen','127.0.0.1');port=int(conf.get('port',9444));interval=int(conf.get('poll_seconds',15))
    if interval<10 or interval>3600:parser.error('poll_seconds must be 10-3600')
    allow_http=bool(conf.get('allow_http_destinations',False))
    store=Store(Path(conf['data_file']).expanduser())
    name=os.environ.get('H910P_NMC_USERNAME','');password=os.environ.get('H910P_NMC_PASSWORD','')
    auth='Basic '+base64.b64encode((name+':'+password).encode()).decode() if name and password else ''
    monitor=Monitor(store,conf['nmc_url'],auth,interval,allow_http)
    srv=Server((listen,port),{'admin_token':admin_token,'allowed_origin':allowed_origin,'allow_http_destinations':allow_http},store,monitor)
    if listen not in ('127.0.0.1','::1'):
        if not conf.get('certfile') or not conf.get('keyfile'):
            parser.error('A TLS certificate/key is required for non-loopback listeners')
    if conf.get('certfile') and conf.get('keyfile'):
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version=ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(conf['certfile'],conf['keyfile']);srv.socket=context.wrap_socket(srv.socket,server_side=True)
    logging.basicConfig(level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
    monitor.start()
    try:srv.serve_forever()
    except KeyboardInterrupt:pass
    finally:
        srv.shutdown();monitor.stop_event.set();monitor.join(timeout=5);srv.server_close()

if __name__=='__main__':main()
