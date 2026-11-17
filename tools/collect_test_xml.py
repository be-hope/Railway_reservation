"""Combine independently executed GoogleTest suites into labelled JUnit evidence."""
import copy,pathlib,xml.etree.ElementTree as ET
root=pathlib.Path(__file__).resolve().parents[1]; reports=root/'docs/reports'
out=ET.Element('testsuite',name='Railway integrated regression')
count=failed=0
for category in ('unit','integration','perf','security'):
    xml=ET.parse(reports/f'normal-{category}.xml').getroot()
    for case in xml.findall('.//testcase'):
        case=copy.deepcopy(case)
        case.set('name',case.get('classname','')+'.'+case.get('name',''))
        props=case.find('properties')
        if props is None: props=ET.SubElement(case,'properties')
        ET.SubElement(props,'property',name='cmake_labels',value=category)
        count+=1; failed+=case.find('failure') is not None
        out.append(case)
out.set('tests',str(count)); out.set('failures',str(failed))
ET.ElementTree(out).write(reports/'ctest-results.xml',encoding='utf-8',xml_declaration=True)
(reports/'normal-tests.log').write_text('\n'.join((reports/f'normal-{kind}.log').read_text() for kind in ('unit','integration','perf','security')))
print(f'Combined {count} executions, {failed} failures')
