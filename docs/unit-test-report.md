# Unit Test Report

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

| Suite | Executed | Passed |
| --- | ---: | ---: |
| unit | 63 | 63 |
| integration | 181 | 181 |
| security | 15 | 15 |
| perf | 8 | 8 |

C++ total: **267**, non-passing: **0**. Separately, 12 real-executable system scenarios ran. Pure unit suite: **63**. Memory/SQLite variants count as separate executions.

The unit suite covers dates/money, fares, seat allocation, PNR generation, input rules, refund boundaries, ticket status transitions and waitlist planning. Unit tests link the business library rather than concrete SQLite repositories. Security/integration checks are identified separately rather than counted as pure unit tests.

| Test | Result | Seconds |
| --- | --- | ---: |
| `Money.FormatsRupeesWithGroupingAndTwoDecimals` | PASS | 0. |
| `Date.ParsesIsoAndDisplayFormats` | PASS | 0. |
| `Date.RejectsImpossibleOrMalformedDates` | PASS | 0. |
| `Date.DayArithmeticCrossesMonthAndYearEnds` | PASS | 0. |
| `ErrorCatalogue.FormatsStandardErrorLine` | PASS | 0. |
| `ErrorCatalogue.EveryCodeHasANameAndMessage` | PASS | 0. |
| `FarePolicy.UT_BKG_01_TotalFareCases` | PASS | 0. |
| `FarePolicy.SeedFaresFromTestPlan` | PASS | 0. |
| `FarePolicy.RoundsHalfUpToThePaisa` | PASS | 0. |
| `FarePolicy.NonPositiveInputsGiveZero` | PASS | 0. |
| `FarePolicy.RateIsConfigurable` | PASS | 0. |
| `SeatAllocator.UT_BKG_04_AllocatesLowestFreeSeatThenExhausts` | PASS | 0. |
| `SeatAllocator.FillsGapsLeftByReleasedSeatsFirst` | PASS | 0. |
| `SeatAllocator.ContinuesIntoTheNextCoach` | PASS | 0. |
| `SeatAllocator.AllOrNothingWhenTooFewSeatsAreFree` | PASS | 0. |
| `SeatAllocator.NeverReturnsAnOccupiedOrDuplicateSeat` | PASS | 0.044 |
| `SeatAllocator.RejectsNonPositiveCount` | PASS | 0. |
| `SeatLabel.FormatsAndParsesLabels` | PASS | 0. |
| `SeatLabel.SeatExistsChecksTheLayout` | PASS | 0. |
| `PnrGenerator.UT_BKG_03_TenThousandUniqueValidPnrs` | PASS | 0.021 |
| `PnrGenerator.UT_BKG_03_CollisionDrawsANewValue` | PASS | 0. |
| `PnrGenerator.GivesUpAfterFiveCollisions` | PASS | 0. |
| `PnrGenerator.PropagatesStorageErrorsFromTheExistsCheck` | PASS | 0. |
| `PnrGenerator.RangeCoversFirstAndLastValues` | PASS | 0. |
| `PnrGenerator.IsValidPnrRejectsMalformedValues` | PASS | 0. |
| `Availability.UT_BKG_05_OneConfirmedLeavesOneFree` | PASS | 0. |
| `Availability.UT_BKG_05_CancelledSeatsAreReleased` | PASS | 0. |
| `Availability.ReportsRacAndWaitlist` | PASS | 0. |
| `Availability.NeverNegativeAfterACapacityChange` | PASS | 0. |
| `StatusDecision.ConfirmedWhenSeatsFreeAndNobodyQueued` | PASS | 0. |
| `StatusDecision.UT_CAN_03_SequenceUntilCapacityIsExhausted` | PASS | 0. |
| `StatusDecision.RacBeforeWaitlist` | PASS | 0. |
| `StatusDecision.PartyLargerThanFreeSeatsIsWaitlistedWhole` | PASS | 0. |
| `StatusDecision.NewBookingDoesNotOvertakeTheWaitlist` | PASS | 0. |
| `StatusDecision.NewBookingDoesNotOvertakeRac` | PASS | 0. |
| `StatusDecision.RejectsZeroTravellers` | PASS | 0. |
| `BookingValidator.UT_BKG_02_TravellerCountBoundaries` | PASS | 0. |
| `BookingValidator.UT_BKG_02_InvalidTravellerFields` | PASS | 0. |
| `BookingValidator.AgeBoundaryValues` | PASS | 0. |
| `BookingValidator.MessageNamesTheTravellerAndField` | PASS | 0. |
| `BookingValidator.JourneyDateWindowIsTodayToTodayPlus120` | PASS | 0. |
| `BookingValidator.RejectsSameSourceAndDestination` | PASS | 0. |
| `BookingValidator.RejectsBadRequestIdTrainAndStation` | PASS | 0. |
| `BookingValidator.HostileInputIsRejectedCleanly` | PASS | 0. |
| `WaitlistPlan.UT_CAN_04_HeadThatDoesNotFitBlocksTheQueue` | PASS | 0. |
| `WaitlistPlan.HeadOfWaitlistGetsTheFreedSeat` | PASS | 0. |
| `WaitlistPlan.RacConfirmedThenWaitlistMovesIntoRac` | PASS | 0. |
| `WaitlistPlan.SeveralTicketsArePromotedInQueueOrder` | PASS | 0. |
| `WaitlistPlan.GapsInWaitlistPositionsAreClosed` | PASS | 0. |
| `WaitlistPlan.BlockedRacHeadAlsoBlocksTheWaitlist` | PASS | 0. |
| `WaitlistPlan.NothingToDoWhenNobodyWaits` | PASS | 0. |
| `RefundCalculatorTest.UT_CAN_01_RefundSlabBoundaries` | PASS | 0. |
| `RefundCalculatorTest.WaitlistedTicketGetsFullRefund` | PASS | 0. |
| `RefundCalculatorTest.RacTicketUsesNormalRefundSlabs` | PASS | 0. |
| `RefundCalculatorTest.HandlesPaiseWithoutFloatingPoint` | PASS | 0. |
| `CancellationEligibilityTest.UT_CAN_02_FutureActiveTicketsCanBeCancelled` | PASS | 0. |
| `CancellationEligibilityTest.UT_CAN_02_AlreadyCancelledTicketIsRejected` | PASS | 0. |
| `CancellationEligibilityTest.UT_CAN_02_DepartedTicketIsRejected` | PASS | 0. |
| `CancellationEligibilityTest.UT_CAN_02_ExactDepartureTimeIsRejected` | PASS | 0. |
| `TicketStatusTest.ValidForwardTransitionsAreAccepted` | PASS | 0. |
| `TicketStatusTest.InvalidBackwardTransitionsAreRejected` | PASS | 0. |
| `TicketStatusTest.CancelledIsTerminal` | PASS | 0. |
| `TicketStatusTest.RepeatedStatusesAreNotTransitions` | PASS | 0. |

Evidence: [JUnit](reports/ctest-results.xml), [normal log](reports/normal-tests.log), [complete catalogue](reports/test-case-catalogue.csv). Sanitizer evidence: sanitizer-unit/integration/perf/security XML and logs in reports/. No Windows-native or hosted-Jenkins result is claimed.
