"""Derive business decision coverage from the archived unfiltered gcovr trace.
Only branches explicitly marked throw=true by GCC/gcovr are excluded. The raw
whole-production trace is retained so the denominator change is auditable.
"""
import copy,json,pathlib,re,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]; out=root/'docs/reports'
data=json.loads((out/'coverage.json').read_text())
data['files']=[copy.deepcopy(f) for f in data['files'] if re.match(r'src/(auth|booking|cancel|admin|security)/|src/common/(date|clock|domain|money|result|session)',f['file'])]
for file in data['files']:
    for line in file['lines']:
        line['branches']=[branch for branch in line.get('branches',[]) if not branch.get('throw',False)]
trace=out/'business-coverage.json'; trace.write_text(json.dumps(data)+'\n')
with tempfile.TemporaryDirectory(prefix='empty-gcov-search-') as empty:
    subprocess.run(['gcovr','--root',str(root),'--add-tracefile',str(trace),'--json-summary',str(out/'business-coverage-summary.json'),'--xml',str(out/'business-coverage.xml'),'--print-summary',empty],cwd=root,check=True)
