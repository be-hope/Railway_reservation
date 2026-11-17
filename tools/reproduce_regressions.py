"""Reproduce two corrected integration regressions in an isolated source copy.
The service sources are archived pre-fix fixtures recorded during integration. The current
regression tests and compatible build glue are copied; the working tree is never modified.
"""
import pathlib, shutil, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
report = root/'docs/reports'
with tempfile.TemporaryDirectory(prefix='railway-before-') as directory:
    stage = pathlib.Path(directory)
    for name in ('src','tools','tests'):
        shutil.copytree(root/name,stage/name)
    shutil.copy2(root/'CMakeLists.txt',stage/'CMakeLists.txt')
    for name in ('src/booking/booking_service.cpp','src/cancel/cancellation_service.cpp'):
        (stage/name).write_bytes((root/'tests/regression_fixtures'/(pathlib.Path(name).name.replace('.cpp','.before.cpp.txt'))).read_bytes())
    with (report/'regression-before-build.log').open('w') as log:
        subprocess.run(['cmake','-S',str(stage),'-B',str(stage/'build'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Debug'],stdout=log,stderr=subprocess.STDOUT,check=True)
        subprocess.run(['cmake','--build',str(stage/'build'),'--target','rrs_integration_tests','-j','4'],stdout=log,stderr=subprocess.STDOUT,check=True)
    result = subprocess.run([str(stage/'build/tests/rrs_integration_tests'),'--gtest_filter=*FinalIntegration.InactiveTrainHasNoAvailabilityAndSameStationHasNoFare*:*FinalIntegration.StaffCancelsWithRefundAndOpsIsDenied*','--gtest_output=xml:'+str(report/'regression-before.xml')],capture_output=True,text=True)
    (report/'regression-before.log').write_text(result.stdout+result.stderr)
    if result.returncode == 0: raise RuntimeError('Expected pre-fix regression failures were not reproduced')
    print('Recorded actual pre-fix failures in docs/reports/regression-before.log and .xml')
