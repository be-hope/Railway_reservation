// seat.cpp
// Sprint 2 - Seat Availability and Allocation
// Closes #25 (Define coach, seat and class-config records)
//
// FR-009: Define class configuration with capacity, RAC and waitlist quotas.
// FR-022: Coach and seat number tracking.
// Tests:  UT-BKG-05

#include <iostream>
#include <string>
#include <vector>
using namespace std;

// ─────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────

// ClassConfig holds capacity rules for one travel class (e.g. "SL", "3A", "2A", "1A").
struct ClassConfig {
    string classCode;       // e.g. "SL", "3A", "2A", "1A"
    int seatsPerCoach;      // confirmed seats per coach
    int racPerCoach;        // RAC (Reservation Against Cancellation) berths per coach
    int waitlistCapacity;   // max waitlist entries across all coaches
    float fareMultiplier;   // multiplier applied to base fare for this class
};

// Seat represents a single physical berth / seat in a coach.
struct Seat {
    int seatNumber;         // 1-based within coach
    string berth;           // "LB" / "MB" / "UB" / "SL" / "RAC" etc.
    bool isBooked;          // true if confirmed booking
    bool isRac;             // true if this is an RAC position
};

// Coach groups seats of one class on a train.
struct Coach {
    string coachId;         // e.g. "S1", "B1", "A1"
    string classCode;       // matches ClassConfig.classCode
    int trainId;
    vector<Seat> seats;
};

// ─────────────────────────────────────────────
// Helper Functions
// ─────────────────────────────────────────────

// Build a Coach populated with the correct number of confirmed + RAC seats.
Coach buildCoach(const string& coachId, const string& classCode,
                 int trainId, const ClassConfig& cfg) {
    Coach coach;
    coach.coachId    = coachId;
    coach.classCode  = classCode;
    coach.trainId    = trainId;

    // Confirmed berths
    for (int i = 1; i <= cfg.seatsPerCoach; i++) {
        Seat s;
        s.seatNumber = i;
        s.berth      = "CONFIRMED";
        s.isBooked   = false;
        s.isRac      = false;
        coach.seats.push_back(s);
    }

    // RAC berths (numbered after confirmed seats)
    for (int i = 1; i <= cfg.racPerCoach; i++) {
        Seat s;
        s.seatNumber = cfg.seatsPerCoach + i;
        s.berth      = "RAC";
        s.isBooked   = false;
        s.isRac      = true;
        coach.seats.push_back(s);
    }

    return coach;
}

// Count free confirmed seats in a coach.
int availableConfirmed(const Coach& coach) {
    int count = 0;
    for (const Seat& s : coach.seats) {
        if (!s.isRac && !s.isBooked) count++;
    }
    return count;
}

// Count free RAC slots in a coach.
int availableRac(const Coach& coach) {
    int count = 0;
    for (const Seat& s : coach.seats) {
        if (s.isRac && !s.isBooked) count++;
    }
    return count;
}

// Total capacity (confirmed + RAC) across all coaches of a class.
void capacitySummary(const vector<Coach>& coaches, const string& classCode,
                     int& totalConfirmed, int& totalRac) {
    totalConfirmed = 0;
    totalRac       = 0;
    for (const Coach& c : coaches) {
        if (c.classCode == classCode) {
            for (const Seat& s : c.seats) {
                if (s.isRac)  totalRac++;
                else          totalConfirmed++;
            }
        }
    }
}

// Book the first available confirmed seat in a coach; returns seat number or -1.
int bookConfirmedSeat(Coach& coach) {
    for (Seat& s : coach.seats) {
        if (!s.isRac && !s.isBooked) {
            s.isBooked = true;
            return s.seatNumber;
        }
    }
    return -1;
}

// Book the first available RAC slot in a coach; returns seat number or -1.
int bookRacSeat(Coach& coach) {
    for (Seat& s : coach.seats) {
        if (s.isRac && !s.isBooked) {
            s.isBooked = true;
            return s.seatNumber;
        }
    }
    return -1;
}

// ─────────────────────────────────────────────
// Display
// ─────────────────────────────────────────────

void displayClassConfig(const ClassConfig& cfg) {
    cout << "─────────────────────────────────────\n";
    cout << "Class        : " << cfg.classCode       << "\n";
    cout << "Seats/Coach  : " << cfg.seatsPerCoach   << "\n";
    cout << "RAC/Coach    : " << cfg.racPerCoach      << "\n";
    cout << "Waitlist Cap : " << cfg.waitlistCapacity << "\n";
    cout << "Fare Mult    : " << cfg.fareMultiplier   << "x\n";
}

void displayCoach(const Coach& coach) {
    cout << "─────────────────────────────────────\n";
    cout << "Coach " << coach.coachId << " (class " << coach.classCode
         << ", train " << coach.trainId << ")\n";
    int avail = availableConfirmed(coach);
    int rac   = availableRac(coach);
    cout << "  Confirmed available : " << avail << "\n";
    cout << "  RAC available       : " << rac   << "\n";
}

// ─────────────────────────────────────────────
// Tests (UT-BKG-05)
// ─────────────────────────────────────────────

void runSeatTests() {
    cout << "\n========== SEAT & COACH TESTS (UT-BKG-05) ==========\n";
    int passed = 0, failed = 0;

    // Standard Indian Railways class configs
    ClassConfig slClass  = {"SL",  72, 8, 200, 1.0f};
    ClassConfig thirdAC  = {"3A",  64, 8, 100, 1.75f};
    ClassConfig secondAC = {"2A",  46, 4,  50, 2.5f};
    ClassConfig firstAC  = {"1A",  24, 2,  20, 4.0f};

    // Test 1: Correct total seats per coach (SL)
    {
        Coach c = buildCoach("S1", "SL", 101, slClass);
        int expectedTotal = slClass.seatsPerCoach + slClass.racPerCoach; // 72+8=80
        bool ok = ((int)c.seats.size() == expectedTotal);
        cout << "[TEST 1] SL coach total berths (" << c.seats.size() << " == " << expectedTotal << "): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 2: Correct total seats per coach (3A)
    {
        Coach c = buildCoach("B1", "3A", 101, thirdAC);
        int expectedTotal = thirdAC.seatsPerCoach + thirdAC.racPerCoach; // 64+8=72
        bool ok = ((int)c.seats.size() == expectedTotal);
        cout << "[TEST 2] 3A coach total berths (" << c.seats.size() << " == " << expectedTotal << "): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 3: All seats start unbooked
    {
        Coach c = buildCoach("A1", "1A", 101, firstAC);
        int available = availableConfirmed(c);
        bool ok = (available == firstAC.seatsPerCoach);
        cout << "[TEST 3] 1A all seats initially available (" << available << " == "
             << firstAC.seatsPerCoach << "): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 4: RAC seats start unbooked
    {
        Coach c = buildCoach("A1", "1A", 101, firstAC);
        int racFree = availableRac(c);
        bool ok = (racFree == firstAC.racPerCoach);
        cout << "[TEST 4] 1A all RAC initially available (" << racFree << " == "
             << firstAC.racPerCoach << "): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 5: Book confirmed seat decrements available count
    {
        Coach c = buildCoach("B1", "3A", 101, thirdAC);
        int seat = bookConfirmedSeat(c);
        int available = availableConfirmed(c);
        bool ok = (seat == 1) && (available == thirdAC.seatsPerCoach - 1);
        cout << "[TEST 5] Book 1 confirmed seat in 3A (seat=" << seat
             << ", avail=" << available << "): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 6: Book all confirmed seats then fail
    {
        Coach c = buildCoach("B2", "3A", 101, thirdAC);
        for (int i = 0; i < thirdAC.seatsPerCoach; i++) bookConfirmedSeat(c);
        int seat = bookConfirmedSeat(c); // should fail
        bool ok = (seat == -1);
        cout << "[TEST 6] No confirmed seat after all booked (returns -1): "
             << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 7: Book RAC slot
    {
        Coach c = buildCoach("S1", "SL", 101, slClass);
        int seat = bookRacSeat(c);
        int racLeft = availableRac(c);
        bool ok = (seat == slClass.seatsPerCoach + 1) && (racLeft == slClass.racPerCoach - 1);
        cout << "[TEST 7] Book 1 RAC berth in SL (seat=" << seat
             << ", racLeft=" << racLeft << "): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 8: Capacity summary across two coaches
    {
        vector<Coach> coaches;
        coaches.push_back(buildCoach("S1", "SL", 101, slClass));
        coaches.push_back(buildCoach("S2", "SL", 101, slClass));
        int totalConf, totalRac;
        capacitySummary(coaches, "SL", totalConf, totalRac);
        bool ok = (totalConf == 2 * slClass.seatsPerCoach) &&
                  (totalRac  == 2 * slClass.racPerCoach);
        cout << "[TEST 8] 2x SL coaches capacity (conf=" << totalConf
             << " rac=" << totalRac << "): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 9: Fare multiplier stored correctly in ClassConfig
    {
        bool ok = (thirdAC.fareMultiplier == 1.75f);
        cout << "[TEST 9] 3A fare multiplier == 1.75: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 10: Waitlist capacity stored correctly
    {
        bool ok = (slClass.waitlistCapacity == 200);
        cout << "[TEST 10] SL waitlist capacity == 200: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    cout << "=====================================================\n";
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

int main() {
    cout << "Seat and coach management module loaded.\n";

    // Show sample class configs
    ClassConfig thirdAC = {"3A", 64, 8, 100, 1.75f};
    displayClassConfig(thirdAC);

    // Build and display a sample coach
    Coach sample = buildCoach("B1", "3A", 101, thirdAC);
    displayCoach(sample);

    runSeatTests();
    return 0;
}
