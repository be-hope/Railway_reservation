"""Inspect application data without changing it or exposing password hashes."""
import pathlib, sqlite3, sys
path = pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'demo.db').resolve()
if not path.is_file(): raise SystemExit('Database not found. Start the application first.')
with sqlite3.connect(path.as_uri()+'?mode=ro', uri=True) as db:
    for table in ('account','station','train','schedule','class_config','reservation','traveller'):
        print(table, db.execute('SELECT COUNT(*) FROM '+table).fetchone()[0])
    print('\nPNR | owner | train | date | status | fare paise | refund paise')
    for row in db.execute('SELECT pnr,account_id,train_number,journey_date,status,fare_paise,refund_paise FROM reservation ORDER BY queue_seq'):
        print(' | '.join(str(value) for value in row))
