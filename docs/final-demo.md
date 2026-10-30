# Final demonstration script

Use a new demo database and a directory without a legacy audit.log. Run from the repository root after following README build instructions.

1. Start `./build/rrs_passenger_demo final-demo.db --seed-demo`.
2. Select Login, use `9876500001` / `Test@1234`. Select Search trains, enter SBC to MAS and a journey date tomorrow in DD-MM-YYYY format. Follow the prompts to select train 20001 and class 3A. Enter two travellers, valid ages/genders and ID proof numbers. The accepted fare should be Rs 1,750.00 for 350 km. Record the generated PNR.
3. Open My bookings. The new reservation appears in journey-date order. Open My profile, complete age/contact/ID fields, and view it again; ID proof shows only its last four characters. Logout.
4. Login as `9876500002` with the same test password. My bookings must not show the first account's reservation. Try its PNR: access must be denied. Logout.
5. Login as `staff01` / `Test@1234`. Choose Book for passenger; use account `acc-p2`, train 20001, SBC to MAS, tomorrow's date, class SL, and a valid traveller. Accept the quoted fare. Record this PNR.
6. Choose Correct traveller name, enter the staff-booked PNR and change the name. View the PNR to confirm it still exists. The integration test verifies owner, fare, seat, and booked-by invariants.
7. Logout. Enter an incorrect password for passenger 9876500001 five times. A correct password is now rejected as locked. After five minutes it succeeds. The automated security test checks the exact 299/300-second boundary without waiting.
8. Exit and restart with the same database filename (without --seed-demo). Log in and view the booking/profile again to show persistence.
9. Inspect audit.log: account events, successful/failed login, lockout, unauthorized attempts, bookings, and reservation corrections have timestamp, actor and chain hash; no passwords or ID-proof numbers should appear.
10. Run `./build/tests/rrs_security_tests` and show SEC-T01 through SEC-T08 results. Then run sequential CTest and the sanitizer/static-analysis commands from README.

The passenger menu's cancellation option reports unavailable in this integration. Final cancellation/refunds and the other members' pending train administration work are separate from the assigned authentication/security issues.
