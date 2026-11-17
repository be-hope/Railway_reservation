@echo off
setlocal
pushd "%~dp0"
wsl -d Ubuntu --cd "%CD%" -- bash -lc "cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-final -j 4 && ./build-final/rrs_app demo.db --seed-demo"
popd
pause
