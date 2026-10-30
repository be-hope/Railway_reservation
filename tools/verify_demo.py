import datetime
import pathlib
import sqlite3
import subprocess
import tempfile

exe = str((pathlib.Path(__file__).resolve().parents[1] / 'build/rrs_passenger_demo').resolve())
date = (datetime.date.today() + datetime.timedelta(days=1)).strftime('%d-%m-%Y')
with tempfile.TemporaryDirectory(prefix='rrs-demo-') as work:
    def run(lines, seed=False):
        args = [exe, str(pathlib.Path(work)/'demo.db')]
        if seed:
            args += ['--seed-demo']
        result = subprocess.run(args, input='\n'.join(lines)+'\n', text=True,
                                capture_output=True, cwd=work, timeout=30)
        assert result.returncode == 0, result.stderr
        return result.stdout
    output = run(['2','9876500001','Test@1234','4','20001',date,'SBC','MAS','3A','2',
                  'Asha Rao','28','F','DL123456789','Ravi Kumar','30','M','DL987654321','Y','5','0','0'], True)
    assert 'Booking saved' in output, output
    assert '1,750.00' in output, output
    db = sqlite3.connect(str(pathlib.Path(work)/'demo.db'))
    pnr, owner, fare = db.execute('SELECT pnr, account_id, fare_paise FROM reservation').fetchone()
    assert owner == 'acc-p1' and fare == 175000
    db.close()
    other = run(['2','9876500002','Test@1234','5','6',pnr,'0','0'])
    assert 'You have no bookings yet.' in other and 'E1006' in other, other
    persisted = run(['2','9876500001','Test@1234','5','0','0'])
    assert pnr in persisted
    staff = run(['2','staff01','Test@1234','2',pnr,'Corrected Asha','0','0'])
    assert 'OK: reservation corrected.' in staff, staff
    log = (pathlib.Path(work)/'audit.log').read_text()
    assert 'Test@1234' not in log and 'DL123456789' not in log
    assert 'RESERVATION_CORRECTED' in log and 'UNAUTHORIZED_ATTEMPT' in log
    print('PASS: CLI login, two-traveller Rs 1750 booking, private history/PNR, restart persistence, staff correction and secret-free audit')
