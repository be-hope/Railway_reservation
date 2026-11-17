"""End-to-end checks against the real executable and temporary SQLite databases."""
import datetime
import json
import pathlib
import sqlite3
import subprocess
import sys
import tempfile
import time
import unittest

EXE = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'build-final/rrs_app').resolve()
REPORT = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else 'tmp/system-results.json')
DATE = (datetime.date.today() + datetime.timedelta(days=3)).strftime('%d-%m-%Y')

class SystemValidation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='railway-system-')
        self.work = pathlib.Path(self.temp.name)
        self.db = self.work / 'system.db'
        self.run_app(['0'], seed=True)
    def tearDown(self):
        self.temp.cleanup()
    def run_app(self, lines, seed=False):
        args = [str(EXE), str(self.db)] + (['--seed-demo'] if seed else [])
        result = subprocess.run(args, input='\n'.join(lines)+'\n', text=True, capture_output=True, cwd=self.work, timeout=45)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout
    def sql(self, statement):
        with sqlite3.connect(self.db) as db:
            return db.execute(statement).fetchall()
    def book(self, login='9876500001', train='20001', destination='MAS', count=1, cls='SL'):
        fields = ['2',login,'Test@1234','4',train,DATE,'SBC',destination,cls,str(count)]
        for i in range(count): fields += ['Traveller '+str(i), '28','F','DL123456789']
        output = self.run_app(fields + ['Y','5','0','0'])
        self.assertIn('Booking saved', output)
        return output
    def test_ST_BKG_01_02_quote_booking_and_history(self):
        output = self.book(count=2, cls='3A')
        self.assertIn('1,750.00', output)
        self.assertEqual(self.sql('SELECT fare_paise, status FROM reservation'), [(175000, 'CONFIRMED')])
    def test_SEC_T08_foreign_history_and_pnr_are_denied(self):
        self.book()
        pnr = self.sql('SELECT pnr FROM reservation')[0][0]
        output = self.run_app(['2','9876500002','Test@1234','5','6',pnr,'0','0'])
        self.assertIn('You have no bookings yet', output)
        self.assertIn('E1006', output)
    def test_ST_CAN_01_cancel_refund_and_repeat_rejection(self):
        self.book()
        pnr = self.sql('SELECT pnr FROM reservation')[0][0]
        output = self.run_app(['2','9876500001','Test@1234','7',pnr,'Y','7',pnr,'Y','0','0'])
        self.assertIn('E4002', output)
        self.assertEqual(self.sql('SELECT status, refund_paise FROM reservation'), [('CANCELLED',31500)])
        self.assertEqual(self.sql('SELECT seat_number FROM traveller'), [(None,)])
    def test_IT_CAN_03_waitlist_promotes_after_cancellation(self):
        for _ in range(3): self.book(train='20003', destination='MYS')
        rows = self.sql('SELECT pnr, status FROM reservation ORDER BY queue_seq')
        self.assertEqual([r[1] for r in rows], ['CONFIRMED','CONFIRMED','WAITLISTED'])
        self.run_app(['2','9876500001','Test@1234','7',rows[0][0],'Y','0','0'])
        self.assertEqual(self.sql('SELECT status FROM reservation ORDER BY queue_seq'), [('CANCELLED',),('CONFIRMED',),('CONFIRMED',)])
    def test_ST_ADM_01_admin_creates_bookable_train(self):
        output = self.run_app(['2','admin01','Test@1234','1','77777','Practice Express',
            '5','77777','2','SBC','0','0','-1','600','MAS','350','0','900','-1',
            '6','77777',DATE,'8','77777','SL','1','2','0','2','100','0','0'])
        self.assertNotIn('ERROR', output)
        self.book(train='77777')
        self.assertEqual(self.sql("SELECT name FROM traveller")[0][0], 'Traveller 0')
        self.assertEqual(self.sql("SELECT train_number FROM reservation"), [('77777',)])
    def test_IT_STF_01_02_staff_book_correct_and_cancel(self):
        output = self.run_app(['2','staff01','Test@1234','3','acc-p1','20001','SBC','MAS',DATE,'SL','Asha','28','F','DL123456','y','0','0'])
        self.assertIn('OK: PNR', output)
        pnr = self.sql('SELECT pnr FROM reservation')[0][0]
        output = self.run_app(['2','staff01','Test@1234','2',pnr,'Corrected Asha','4',pnr,'0','0'])
        self.assertIn('reservation corrected', output)
        self.assertEqual(self.sql('SELECT booked_by, status FROM reservation'), [('acc-s1','CANCELLED')])
        self.assertEqual(self.sql('SELECT name FROM traveller'), [('Corrected Asha',)])
    def test_FR025_ops_reports_match_stored_booking(self):
        self.book()
        output = self.run_app(['2','ops01','Test@1234','9','0','0'])
        self.assertIn('20001', output)
        self.assertIn('350.00', output)
    def test_AUTH_registration_login_and_restart(self):
        output = self.run_app(['1','New Passenger','9123456789','new@example.com','Password123','2','9123456789','Password123','5','0','0'])
        self.assertIn('registered', output)
        self.assertIn('You have no bookings yet', output)
        output = self.run_app(['2','new@example.com','Password123','5','0','0'])
        self.assertIn('You have no bookings yet', output)
        digest = self.sql("SELECT password_hash FROM account WHERE contact_number='9123456789'")[0][0]
        self.assertEqual(len(digest),64)
        self.assertNotIn('Password123', (self.work/'audit.log').read_text())
    def test_AUTH_invalid_login_and_secret_free_audit(self):
        output = self.run_app(['2','9876500001','wrong','0'])
        self.assertIn('E1003',output)
        self.assertNotIn('wrong',(self.work/'audit.log').read_text())
    def test_MNT_restart_retains_ticket(self):
        self.book()
        pnr = self.sql('SELECT pnr FROM reservation')[0][0]
        output = self.run_app(['2','9876500001','Test@1234','5','0','0'])
        self.assertIn(pnr,output)

    def test_ADMIN_cancellation_is_authorized(self):
        self.book()
        pnr = self.sql('SELECT pnr FROM reservation')[0][0]
        output = self.run_app(['2','admin01','Test@1234','14',pnr,'0','0'])
        self.assertIn('cancellation saved',output)
        self.assertEqual(self.sql('SELECT status FROM reservation'), [('CANCELLED',)])
    def test_OPS_cannot_modify_trains(self):
        output = self.run_app(['2','ops01','Test@1234','1','0','0'])
        self.assertIn('administrator permission required',output)
        self.assertEqual(self.sql('SELECT COUNT(*) FROM train'),[(4,)])

class RecordingResult(unittest.TextTestResult):
    def startTest(self, test):
        self.started = time.monotonic()
        super().startTest(test)
    def stopTest(self, test):
        records.append({'case':test.id(),'status':'FAIL' if any(t == test for t,_ in self.failures+self.errors) else 'PASS','seconds':round(time.monotonic()-self.started,3)})
        super().stopTest(test)

records = []
suite = unittest.defaultTestLoader.loadTestsFromTestCase(SystemValidation)
result = unittest.TextTestRunner(verbosity=2,resultclass=RecordingResult).run(suite)
REPORT.parent.mkdir(parents=True,exist_ok=True)
REPORT.write_text(json.dumps({'executed_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'executable':EXE.name,'tests':records},indent=2)+'\n')
sys.exit(not result.wasSuccessful())
