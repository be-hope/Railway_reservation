# Final demonstration walkthrough

Use the actual execution date, not the historical sprint deadline. All scenarios below are automated in `tools/system_validation.py`; the numbered steps are for a live demonstration to the TA.

1. Double-click `Start-Railway.cmd`, or run `./build-final/rrs_app demo.db --seed-demo` in Ubuntu at the repository root. The login screen appears. Reuse an existing database to show persistence, or choose a new filename for a clean demo.
2. Login as `9876500001`, password `Test@1234`. Choose 4 Book ticket. Train: 20001; date: three days from today (DD-MM-YYYY); source SBC; destination MAS; class 3A; two travellers with names, ages 28/30, genders F/M and IDs DL123456789/DL987654321. Accept the fare. Show Rs 1,750 and the generated ten-digit PNR.
3. Choose 5 My bookings, then 6 PNR status. Explain CONFIRMED and seat numbers. Choose 8 My profile, fill age/contact/ID details and display the profile again to show masking. Logout.
4. Login as `9876500002`. My bookings should not show the first ticket. Enter its PNR: E1006 (forbidden). Explain ownership/RBAC. Logout.
5. Login as passenger 1 and cancel that PNR with option 7. For a departure at least 48 hours away, show a 90% refund: Rs 1,575 for Rs 1,750. Repeat cancellation: E4002. Show the stored ticket is CANCELLED and seats are released.
6. Waitlist: book three single-person tickets on train 20003, SBC to MYS, class SL, for the same future date. It has two seats: the third ticket becomes WAITLISTED. Cancel a confirmed ticket; the waiting ticket becomes CONFIRMED. Automated checks verify this without manual SQL changes.
7. Login as `staff01`. Book for passenger `acc-p2`, correct a traveller's name using the PNR, and cancel an eligible ticket. Explain that booked_by records staff while account_id records the owning passenger. Passenger accounts cannot use these staff screens.
8. Login as `admin01`. Add train 77777, name Practice Express; set two stops: SBC, km0, day0, arrival -1, departure600; MAS, km350, day0, arrival900, departure-1. Add a future schedule. Configure SL: coaches1, seats2, RAC0, waitlist2, multiplier100. Logout and book the new train as a passenger. With active future bookings, administrator deactivation/route removal is rejected.
9. Login as `ops01`; choose 9 Reports, then 11 Occupancy with a train/date. Explain these totals come from SQLite through the report service. Ops has report permission, not train-management permission.
10. Exit and restart with the same database; show the ticket still exists. Run `python3 tools/inspect_database.py demo.db` to display backend tables and reservation rows without passwords.
11. Show the GitHub project board, commit history and merged integration PR. Explain issue -> branch -> commit -> push -> PR -> CI -> review/merge -> Done. Do not present a board status as proof that a test passed: use the actual report artifacts.
12. Show one test: `./build-final/tests/rrs_integration_tests --gtest_filter='*FinalIntegration.StaffCancelsWithRefundAndOpsIsDenied*'`. Explain fixture, action and assertions. Open `docs/reports/regression-before.log` and `regression-after.log` to compare the same newly added regressions against pre-fix and corrected service versions.
13. Open the four required reports and `docs/reports/coverage.html`. Explain that pass rate, line coverage and branch coverage are different measures, and quote the recorded values rather than saying 100% coverage.

If asked about login lockout, the automated fixed-clock test checks 299/300-second boundaries without a five-minute live wait. For system tests run `python3 tools/system_validation.py build-final/rrs_app`.
