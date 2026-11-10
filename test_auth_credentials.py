#!/usr/bin/env python3
"""Regression for the H910P double-login Chromium challenge failure.

Browser tests also exercise the actual sign-in code with mocked HTTP responses;
these static checks verify that all explicit card API requests disable implicit
browser HTTP auth negotiation while continuing to send the user-entered Basic
Authorization header. Live NMC HTTP response testing remains necessary.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parent
SRC=ROOT/'source'

class BrowserAuthContract(unittest.TestCase):
    def test_form_login_uses_explicit_authorization_without_browser_auth_cache(self):
        src=(SRC/'ui-auth.js').read_text()
        self.assertIn("fetch('/app_upload.htm'",src)
        self.assertIn("credentials:'omit'",src)
        self.assertIn("headers:{Authorization:header}",src)
        self.assertNotRegex(src,r"fetch\('/app_upload\.htm'.*credentials:'same-origin'")
        self.assertIn("redirect:'error'",src)
        self.assertIn('if(r.status===401||r.status===403)',src)
        self.assertIn('Sign-in timed out.',src)

    def test_authenticated_nmc_requests_do_not_solicit_browser_credentials(self):
        src=(SRC/'ui-data.js').read_text()
        self.assertIn("credentials:'omit'",src)
        self.assertIn('options.headers.Authorization=U.auth.header',src)
        for filename in ['ui-diagnostics.js','ui-updates.js']:
            body=(SRC/filename).read_text()
            self.assertNotIn("credentials:'same-origin'",body,filename)
            self.assertIn("credentials:'omit'",body,filename)

    def test_tab_scoped_session_is_revalidated_and_lock_clears(self):
        src=(SRC/'ui-auth.js').read_text()
        self.assertNotIn('localStorage',src)
        self.assertNotIn('document.cookie',src)
        self.assertIn('sessionStorage.setItem',src)
        self.assertIn('sessionStorage.getItem',src)
        self.assertIn('sessionStorage.removeItem',src)
        self.assertIn('U.checkAuth(cached[1])',src)
        self.assertIn('U.forget();U.auth.header=',src)
        self.assertIn("U.login=function()",src)

    def test_xml_transport_is_serialized_and_reads_retried(self):
        src=(SRC/'ui-data.js').read_text()
        self.assertIn('U.ioQueue=Promise.resolve()',src)
        self.assertIn('U.ioQueue.then(run,run)',src)
        self.assertIn('U.readXML=function(',src)
        self.assertIn('U.pollFailures>=2',src)
        config=(SRC/'ui-config.js').read_text()
        self.assertIn("U.readXML('/configuration_descriptor.xml'",config)

if __name__=='__main__':
    unittest.main(verbosity=2)
