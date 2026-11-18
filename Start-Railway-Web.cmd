@echo off
setlocal
pushd "%~dp0"
echo Starting Railway. When ready, open http://localhost:8765 in your browser.
wsl -d Ubuntu --cd "%CD%" -- bash -lc "cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-final --target rrs_web_bridge -j 4 && python3 web/server.py --seed-demo"
popd
pause
