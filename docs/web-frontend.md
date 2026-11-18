# Local passenger frontend

Run `Start-Railway-Web.cmd` on Windows with the same Ubuntu/WSL prerequisites as the console application. Open http://localhost:8765 after the ready message. On Linux:

```sh
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-final --target rrs_web_bridge -j 4
python3 web/server.py --seed-demo
```

Use `9876500001` / `Test@1234` for the seeded passenger, or register a new account. Search SBC (Bengaluru) to MAS (Chennai) for tomorrow. Select a train, add 1–6 travellers, review the total fare and confirm. My bookings displays persisted tickets, current PNR status and cancellation with the calculated refund.

The browser uses plain HTML, CSS and JavaScript. A Python standard-library server passes requests to a persistent C++ process through a private line protocol. The process calls the same authentication, search, fare, booking, history and cancellation services as the console. SQLite storage defaults to `web-demo.db`, separate from the console demo; restart the server to verify persistence. To use a specific existing database, pass `--database path/to/database.db`. Demo seeding is explicit and only occurs if the seed passenger is absent. An old seeded database retains its original train schedule dates; use a fresh database filename for current-date demo schedules.

This frontend is for local passenger demonstrations. Staff, admin and operations workflows remain in the console. The HTTP listener binds only to 127.0.0.1; same-origin JSON checks and HttpOnly/SameSite cookies protect local requests. Sessions and business permissions are validated by C++, and proof numbers are not returned to the browser. Session expiry requires login again. The single-process server serializes requests; it is not an Internet deployment server. Use a proper HTTPS application server and deployment hardening before public hosting.

Run the additional HTTP integration test with `python3 tests/web/test_frontend.py --bridge ./build-final/rrs_web_bridge`. The previously archived coverage reports describe the earlier console release and do not measure this new adapter.
