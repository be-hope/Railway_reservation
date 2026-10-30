// fare.cpp
// Sprint 2 - Fare Calculation and Booking Quote
// Closes #37 (Implement FareCalculator and unit tests)
// Closes #38 (Implement fare quote for booking)
//
// FR-011: Calculate total fares using fixed-point money and class multipliers.
//         Acceptance criterion: 350 km, class 3A, 2 travellers → Rs 1750 total.
// Tests:  UT-BKG-01 (fare calculation), ST-BKG-02 (quoted fare matches stored fare)
//
// Fare formula (integer arithmetic, paise):
//   farePerPerson = BASE_RATE_PAISE_PER_KM * distanceKm * multiplierTenths / 10
//   Minimum floor: MIN_FARE_PAISE (Rs 50)
//
// Derivation of constants:
//   We need 350 km × 3A × 2 travellers = Rs 1750 total → Rs 875 per person.
//   Rs 875 / 350 km / 2.5 (3A multiplier) = Rs 1.00/km base rate.
//   So BASE_RATE_PAISE_PER_KM = 100 (Rs 1.00).
//   3A multiplierTenths = 25 (i.e. 2.5 ×).

#include <iostream>
#include <string>
#include <vector>
using namespace std;

// ─────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────

// Base rate: Rs 1.00 per km = 100 paise per km.
const int BASE_RATE_PAISE_PER_KM = 100;

// Minimum fare floor: Rs 50 = 5000 paise.
const int MIN_FARE_PAISE = 5000;

// ─────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────

// ClassMultiplier stores the fare multiplier for one travel class.
// multiplierTenths: e.g. 25 means 2.5× (stored ×10 to keep arithmetic integer).
struct ClassMultiplier {
    string classCode;
    int    multiplierTenths;
};

// FareQuote is produced for the passenger to review before confirming booking (#38).
struct FareQuote {
    int    routeId;
    int    scheduleId;
    string classCode;
    int    distanceKm;
    int    numTravellers;
    long   farePaisePer;     // fare per traveller in paise
    long   totalFarePaise;   // farePaisePer × numTravellers  (-1 on error)
};

// ─────────────────────────────────────────────
// Class Multiplier Table
// ─────────────────────────────────────────────

static const ClassMultiplier CLASS_TABLE[] = {
    {"SL",  10},    // 1.0× (sleeper — cheapest)
    {"3A",  25},    // 2.5× (3-tier AC)
    {"2A",  35},    // 3.5× (2-tier AC)
    {"1A",  70},    // 7.0× (first AC)
};
static const int NUM_CLASSES = 4;

// Return multiplierTenths for classCode, or -1 if not found.
int getMultiplierTenths(const string& classCode) {
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (CLASS_TABLE[i].classCode == classCode)
            return CLASS_TABLE[i].multiplierTenths;
    }
    return -1;
}

// ─────────────────────────────────────────────
// FareCalculator
// ─────────────────────────────────────────────

// Calculate fare per traveller in paise.
// Returns -1 on invalid input (unknown class, non-positive distance).
long calculateFarePerPerson(int distanceKm, const string& classCode) {
    if (distanceKm <= 0) {
        cout << "Error: distance must be positive.\n";
        return -1;
    }
    int multTenths = getMultiplierTenths(classCode);
    if (multTenths < 0) {
        cout << "Error: unknown class code '" << classCode << "'.\n";
        return -1;
    }
    // farePerPerson_paise = BASE × dist × (multTenths / 10)
    //                     = BASE × dist × multTenths / 10  (integer division last)
    long fare = (long)BASE_RATE_PAISE_PER_KM * distanceKm * multTenths / 10;
    if (fare < MIN_FARE_PAISE) fare = MIN_FARE_PAISE;
    return fare;
}

// Calculate total fare in paise for numTravellers.
// Returns -1 on error.
long calculateTotalFare(int distanceKm, const string& classCode, int numTravellers) {
    if (numTravellers <= 0) {
        cout << "Error: number of travellers must be positive.\n";
        return -1;
    }
    long perPerson = calculateFarePerPerson(distanceKm, classCode);
    if (perPerson < 0) return -1;
    return perPerson * numTravellers;
}

// Convert paise to rupees (display only).
float paiseToRupees(long paise) {
    return (float)paise / 100.0f;
}

// ─────────────────────────────────────────────
// Fare Quote — issue #38 (ST-BKG-02)
// ─────────────────────────────────────────────

// Build a FareQuote so the passenger can confirm the fare before booking.
// totalFarePaise == -1 signals an error (bad class / invalid inputs).
FareQuote buildFareQuote(int routeId, int scheduleId,
                         const string& classCode,
                         int distanceKm, int numTravellers) {
    FareQuote q;
    q.routeId       = routeId;
    q.scheduleId    = scheduleId;
    q.classCode     = classCode;
    q.distanceKm    = distanceKm;
    q.numTravellers = numTravellers;
    q.farePaisePer  = calculateFarePerPerson(distanceKm, classCode);
    if (q.farePaisePer < 0) {
        q.totalFarePaise = -1;
    } else {
        q.totalFarePaise = q.farePaisePer * numTravellers;
    }
    return q;
}

void displayFareQuote(const FareQuote& q) {
    cout << "─────────────────────────────────────\n";
    cout << "Fare Quote\n";
    cout << "  Route / Schedule : " << q.routeId << " / " << q.scheduleId << "\n";
    cout << "  Class            : " << q.classCode << "\n";
    cout << "  Distance         : " << q.distanceKm << " km\n";
    cout << "  Travellers       : " << q.numTravellers << "\n";
    if (q.totalFarePaise < 0) {
        cout << "  Fare             : ERROR\n";
    } else {
        cout << "  Per person       : Rs " << paiseToRupees(q.farePaisePer) << "\n";
        cout << "  Total fare       : Rs " << paiseToRupees(q.totalFarePaise) << "\n";
    }
}

// ─────────────────────────────────────────────
// Tests (UT-BKG-01 and ST-BKG-02)
// ─────────────────────────────────────────────

void runFareTests() {
    cout << "\n========== FARE TESTS (UT-BKG-01 / ST-BKG-02) ==========\n";
    int passed = 0, failed = 0;

    // Test 1: Acceptance criterion from issue #37
    // 350 km, 3A, 2 travellers → Rs 1750 total
    // Per person: 100 paise/km × 350 km × 25/10 = 87500 paise = Rs 875
    // Total: Rs 875 × 2 = Rs 1750
    {
        long total = calculateTotalFare(350, "3A", 2);
        long expected = 175000; // Rs 1750 = 175000 paise
        bool ok = (total == expected);
        cout << "[TEST 1] 350km, 3A, 2 travellers = Rs "
             << paiseToRupees(total) << " (expect Rs 1750): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 2: Zero distance rejected
    {
        long f = calculateFarePerPerson(0, "SL");
        bool ok = (f == -1);
        cout << "[TEST 2] Zero distance rejected: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 3: Negative distance rejected
    {
        long f = calculateFarePerPerson(-10, "SL");
        bool ok = (f == -1);
        cout << "[TEST 3] Negative distance rejected: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 4: Unknown class rejected
    {
        long f = calculateFarePerPerson(100, "FC");
        bool ok = (f == -1);
        cout << "[TEST 4] Unknown class code rejected: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 5: Zero travellers rejected
    {
        long f = calculateTotalFare(100, "SL", 0);
        bool ok = (f == -1);
        cout << "[TEST 5] Zero travellers rejected: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 6: SL fare cheaper than 3A for same distance
    {
        long sl = calculateFarePerPerson(500, "SL");
        long a3 = calculateFarePerPerson(500, "3A");
        bool ok = (sl > 0 && a3 > 0 && sl < a3);
        cout << "[TEST 6] SL (Rs " << paiseToRupees(sl)
             << ") < 3A (Rs " << paiseToRupees(a3) << "): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 7: 1A more expensive than 2A
    {
        long a2 = calculateFarePerPerson(500, "2A");
        long a1 = calculateFarePerPerson(500, "1A");
        bool ok = (a1 > a2);
        cout << "[TEST 7] 1A (Rs " << paiseToRupees(a1)
             << ") > 2A (Rs " << paiseToRupees(a2) << "): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 8: Minimum fare floor applied for very short trip
    {
        long f = calculateFarePerPerson(1, "SL");
        bool ok = (f == MIN_FARE_PAISE); // 1 km SL below floor → Rs 50
        cout << "[TEST 8] Min fare floor for 1 km SL = Rs "
             << paiseToRupees(f) << " (expect Rs 50): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 9: FareQuote total matches direct calculation (ST-BKG-02)
    {
        FareQuote q  = buildFareQuote(1, 10, "2A", 600, 3);
        long direct  = calculateTotalFare(600, "2A", 3);
        bool ok = (q.totalFarePaise == direct && direct > 0);
        cout << "[TEST 9] FareQuote matches calculateTotalFare (Rs "
             << paiseToRupees(q.totalFarePaise) << "): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 10: FareQuote per-person × count == total
    {
        FareQuote q = buildFareQuote(1, 10, "SL", 400, 4);
        bool ok = (q.farePaisePer > 0 &&
                   q.farePaisePer * q.numTravellers == q.totalFarePaise);
        cout << "[TEST 10] FareQuote per-person × count == total: "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 11: FareQuote with invalid class returns error sentinel
    {
        FareQuote q = buildFareQuote(1, 10, "XX", 300, 2);
        bool ok = (q.totalFarePaise == -1);
        cout << "[TEST 11] FareQuote with unknown class returns -1: "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 12: Scaling — total doubles when travellers double
    {
        long one = calculateTotalFare(200, "3A", 1);
        long two = calculateTotalFare(200, "3A", 2);
        bool ok = (one > 0 && two == 2 * one);
        cout << "[TEST 12] Total scales linearly with traveller count: "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    cout << "==========================================================\n";
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

int main() {
    cout << "Fare calculator module loaded.\n";

    // Demo: build and display the acceptance-criterion quote
    FareQuote demo = buildFareQuote(1, 10, "3A", 350, 2);
    displayFareQuote(demo);

    runFareTests();
    return 0;
}
