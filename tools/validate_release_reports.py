"""Validate report consistency and record checksums for the submitted evidence."""
import hashlib,json,pathlib,xml.etree.ElementTree as ET
root=pathlib.Path(__file__).resolve().parents[1]; reports=root/'docs/reports'
normal=ET.parse(reports/'ctest-results.xml').getroot(); cases=normal.findall('.//testcase')
assert cases and not normal.findall('.//failure') and not normal.findall('.//error') and not normal.findall('.//skipped')
assert int(normal.get('tests'))==len(cases)
for suite in ('unit','integration','perf','security'):
    data=ET.parse(reports/f'sanitizer-{suite}.xml').getroot()
    assert not data.findall('.//failure') and not data.findall('.//error') and not data.findall('.//skipped')
    coverage=ET.parse(reports/f'coverage-{suite}.xml').getroot()
    assert not coverage.findall('.//failure') and not coverage.findall('.//error')
system=json.loads((reports/'system-results.json').read_text())['tests']; assert len(system)>=12 and all(t['status']=='PASS' for t in system)
business=json.loads((reports/'business-coverage-summary.json').read_text()); assert business['line_percent']>=80 and business['branch_percent']>=70
before=ET.parse(reports/'regression-before.xml').getroot(); after=ET.parse(reports/'regression-after.xml').getroot()
assert len(before.findall('.//failure'))>=4 and not after.findall('.//failure')
manifest={str(file.relative_to(root)):hashlib.sha256(file.read_bytes()).hexdigest() for file in sorted(reports.rglob('*')) if file.is_file() and file.name!='manifest.json'}
(reports/'manifest.json').write_text(json.dumps({'algorithm':'SHA256','artifacts':manifest},indent=2)+'\n')
print(f'PASS: {len(cases)} C++ tests, {len(system)} system scenarios, sanitizers, coverage thresholds, real before/after failures; {len(manifest)} artifact checksums')
