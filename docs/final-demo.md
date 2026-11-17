# Application walkthrough

Build and run the application using the README instructions. The scenarios below describe the supported workflows and their expected results. Automated equivalents are in `tools/system_validation.py`.

## Start and sign in

Double-click `Start-Railway.cmd`, or run `./build-final/rrs_app demo.db --seed-demo` in Ubuntu at the repository root. A new demonstration database enables the sample accounts listed in README. Reusing the same database preserves bookings and profiles.

## Passenger booking and profile

1. Login as `9876500001` with the demonstration password `Test@1234`.
2. Choose 4 Book ticket. Enter train 20001, a date three days from today in DD-MM-YYYY format, source SBC, destination MAS, class 3A and two travellers. Supply valid names, ages, genders and ID numbers.
3. Review and accept the Rs 1,750 fare. The application returns a ten-digit PNR and allocated seats for a confirmed reservation.
4. Choose 5 My bookings or 6 PNR status to retrieve the ticket. Choose 8 My profile to update profile fields; ID numbers are masked when displayed.
5. Login as `9876500002`. The first passenger's booking is absent from this account's history, and looking up its PNR returns E1006 (forbidden).

## Cancellation and waitlist

Cancel an eligible PNR using passenger option 7. At least 48 hours before departure, the refund is 90%: Rs 1,575 for a Rs 1,750 ticket. The stored status becomes CANCELLED and seats are released. Repeating cancellation returns E4002. Cancellation at or after departure is rejected.

For waitlist behavior, book three single-person tickets on train 20003, SBC to MYS, class SL, for the same future date. Its two confirmed seats fill; the third reservation is WAITLISTED. Cancelling a confirmed reservation promotes the waiting reservation to CONFIRMED.

## Staff reservations

Login as `staff01`. Staff can book for passenger IDs `acc-p1` or `acc-p2`, correct a traveller's name using the PNR and cancel eligible reservations. The reservation's `booked_by` field records the staff account while `account_id` records the owning passenger. Corrections preserve the owner, fare and allocated seats. Passenger accounts cannot use staff operations.

## Administrator maintenance

Login as `admin01` to add, rename, activate or deactivate trains; configure routes, schedules and class capacity; inspect PNRs; correct reservations; cancel eligible tickets; and view reports.

Example train setup:

- Number: 77777; name: Demo Express.
- Origin: SBC, cumulative km 0, day offset 0, arrival -1, departure 600.
- Destination: MAS, cumulative km 350, day offset 0, arrival 900, departure -1.
- Add a future schedule date.
- Configure SL: 1 coach, 2 seats per coach, RAC 0, waitlist capacity 2, fare multiplier 100%.

The new train can then be booked by a passenger. Active future bookings prevent deactivation and route/schedule changes that would invalidate reservations. Capacity cannot be reduced below the booked load.

## Operations reports and persistence

Login as `ops01`. Option 9 displays booking statistics and train-wise counts; option 11 displays occupancy for a train and date. Operations accounts can read reports but cannot modify trains.

Exit and restart with the same database filename to retrieve persisted records. Inspect table counts and reservations without exposing credentials:

```sh
python3 tools/inspect_database.py demo.db
```

## Automated verification

```sh
ctest --test-dir build-final --output-on-failure
python3 tools/system_validation.py build-final/rrs_app
```

The test-case document and four test reports under `docs/` describe coverage, expected outcomes and recorded execution results. Raw evidence is stored under `docs/reports/`.
