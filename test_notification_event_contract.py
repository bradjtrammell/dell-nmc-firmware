"""Provider-specific subscriptions and edge transition tests (no NMC writes)."""
import importlib.util
from pathlib import Path
import unittest

SPEC=importlib.util.spec_from_file_location('h910p_relay',Path(__file__).parent/'notification-relay'/'relay.py')
relay=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(relay)

class EventsTest(unittest.TestCase):
    def test_subscriptions_are_validated_and_returned_without_tokens(self):
        item=relay.destination_spec({'type':'monita','name':'UPS','url':'https://monita.example','token':'verysecret','events':['power_lost','connection_lost']})
        self.assertEqual(relay.public_destination({'id':'a',**item})['events'],['power_lost','connection_lost'])
        self.assertNotIn('token',relay.public_destination({'id':'a',**item}))
        for invalid in [['power_lost','power_lost'],['invalid'],None,'power_lost']:
            with self.assertRaises(relay.ValidationError):relay.destination_spec({'type':'monita','name':'UPS','url':'https://monita.example','token':'t','events':invalid})
    def test_state_changes_emit_once_with_battery_hysteresis(self):
        m=relay.Monitor(None,'http://127.0.0.1','',15,False)
        got=[];m.emit=lambda *args:got.append(args)
        def update(power,charge):m.update({'UPS.PowerSummary.PresentStatus.ACPresent':power,'UPS.PowerSummary.RemainingCapacity':str(charge)})
        update('1',100);update('0',99);update('0',20);update('0',23);update('0',25);update('1',26)
        self.assertEqual([x[0] for x in got],['power_lost','battery_low','battery_recovered','power_restored'])
        with self.assertRaises(ValueError):update('',50)
        self.assertEqual(m.state['power'],'1')
    def test_old_destinations_retain_all_events_default(self):
        self.assertEqual(len(relay.public_destination({'id':'abc','name':'legacy','type':'discord','enabled':True,'url':'https://discord.com/api/webhooks/1/a'})['events']),len(relay.EVENT_TYPES))

if __name__=='__main__':unittest.main()
