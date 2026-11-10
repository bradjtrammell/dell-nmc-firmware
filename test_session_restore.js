'use strict';
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const script = fs.readFileSync(__dirname + '/source/ui-auth.js', 'utf8');

function createSession(storage, valid = true) {
  let started = 0, prompts = 0;
  const app = { innerHTML: '' };
  const inputs = { focus() {}, addEventListener() {} };
  const U = {
    config: { saving: '', values: {}, loaded: false }, power: { busy: false },
    data: {}, events: [], history: [], updated: 0, identity: {}, timer: 0,
    ico: () => '', esc: s => String(s), startApp: () => { started++; }
  };
  const sessionStorage = {
    getItem: key => storage.has(key) ? storage.get(key) : null,
    setItem: (key, val) => { storage.set(key, val); },
    removeItem: key => { storage.delete(key); }
  };
  const ctx = vm.createContext({
    U, document: { getElementById: id => id === 'app' ? app : inputs },
    location: { hostname: '192.168.0.174' },
    sessionStorage, console, AbortController, setTimeout, clearTimeout, setInterval, clearInterval,
    btoa: s => Buffer.from(s, 'binary').toString('base64'),
    unescape, encodeURIComponent,
    fetch: (_path, options) => {
      assert.equal(options.credentials, 'omit');
      assert.equal(options.redirect, 'error');
      assert.match(options.headers.Authorization, /^Basic /);
      prompts++;
      return Promise.resolve({status: valid ? 200 : 401, ok: valid});
    }
  });
  vm.runInContext(script, ctx);
  U.startApp = () => { started++; };
  return {U, storage, app, get started() {return started;}, get checks() {return prompts;}};
}
const tick = async () => {await Promise.resolve();await new Promise(setImmediate);};
(async () => {
  const storage = new Map();
  const initial = createSession(storage);
  initial.U.auth.name = 'admin';
  initial.U.auth.header = 'Basic ' + Buffer.from('admin:admin').toString('base64');
  initial.U.remember();
  assert.equal(storage.size, 1);
  const reloaded = createSession(storage);
  reloaded.U.login();
  await tick();
  assert.equal(reloaded.started, 1, 'reload must reopen dashboard');
  assert.equal(reloaded.U.auth.header, initial.U.auth.header);
  assert.equal(reloaded.checks, 1, 'reload must revalidate credentials exactly once');
  reloaded.U.lockInterface();
  assert.equal(storage.size, 0, 'Lock must clear cached tab credentials');
  assert.equal(reloaded.U.auth.header, '');
  const next = createSession(storage);
  next.U.login();
  assert.equal(next.started, 0, 'Lock must prevent restore after reload');
  initial.U.remember();
  const rejected = createSession(storage, false);
  rejected.U.login();
  await tick();
  assert.equal(rejected.started, 0);
  assert.equal(storage.size, 0, 'Rejected session must be discarded');
  console.log('PASS: same-tab restore, authenticated revalidation, Lock wipe, rejected-session wipe');
})().catch(err => {console.error(err);process.exitCode = 1;});
