// train.cpp
// Sprint 1 - Train Module
// Closes #6 (Define train data structure)
// Closes #7 (Implement train search by source/destination/date)
// Closes #8 (Unit tests for train search)

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

// ─────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────

struct Train {
    int    trainId;
    string trainName;
    string source;
    string destination;
    string departureTime;   // "HH:MM"
    string arrivalTime;     // "HH:MM"
    string runningDays;     // e.g. "Mon,Tue,Wed,Thu,Fri"
    int    totalSeats;
    int    availableSeats;
    float  fare;
};

// ─────────────────────────────────────────────
// Input Validation
// ─────────────────────────────────────────────

bool isValidTimeFormat(const string& t) {
    if (t.size() != 5 || t[2] != ':') return false;
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;
        if (t[i] < '0' || t[i] > '9') return false;
    }
    int hh = stoi(t.substr(0, 2));
    int mm = stoi(t.substr(3, 2));
    return hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59;
}

bool isValidTrain(const Train& tr) {
    if (tr.trainId <= 0)              { cout << "Error: trainId must be positive.\n";        return false; }
    if (tr.trainName.empty())          { cout << "Error: trainName cannot be empty.\n";       return false; }
    if (tr.source.empty())             { cout << "Error: source cannot be empty.\n";          return false; }
    if (tr.destination.empty())        { cout << "Error: destination cannot be empty.\n";     return false; }
    if (tr.source == tr.destination)   { cout << "Error: source and destination are same.\n"; return false; }
    if (!isValidTimeFormat(tr.departureTime)) { cout << "Error: invalid departure time.\n";  return false; }
    if (!isValidTimeFormat(tr.arrivalTime))   { cout << "Error: invalid arrival time.\n";    return false; }
    if (tr.totalSeats <= 0)            { cout << "Error: totalSeats must be positive.\n";     return false; }
    if (tr.availableSeats < 0 || tr.availableSeats > tr.totalSeats) {
        cout << "Error: availableSeats out of range.\n"; return false;
    }
    if (tr.fare < 0)                   { cout << "Error: fare cannot be negative.\n";         return false; }
    return true;
}

// ─────────────────────────────────────────────
// Search
// ─────────────────────────────────────────────

// Case-insensitive compare
string toLower(const string& s) {
    string r = s;
    transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

vector<Train> searchTrains(const vector<Train>& trains,
                           const string& source,
                           const string& destination) {
    vector<Train> results;
    string srcL = toLower(source);
    string dstL = toLower(destination);
    for (const Train& t : trains) {
        if (toLower(t.source) == srcL && toLower(t.destination) == dstL) {
            results.push_back(t);
        }
    }
    return results;
}

vector<Train> searchTrainsByDay(const vector<Train>& trains,
                                const string& source,
                                const string& destination,
                                const string& day) {
    vector<Train> base   = searchTrains(trains, source, destination);
    vector<Train> result;
    string dayL = toLower(day);
    for (const Train& t : base) {
        if (toLower(t.runningDays).find(dayL) != string::npos) {
            result.push_back(t);
        }
    }
    return result;
}

// ─────────────────────────────────────────────
// Display
// ─────────────────────────────────────────────

void displayTrain(const Train& t) {
    cout << "─────────────────────────────────────\n";
    cout << "Train ID   : " << t.trainId      << "\n";
    cout << "Name       : " << t.trainName    << "\n";
    cout << "Route      : " << t.source << " → " << t.destination << "\n";
    cout << "Departure  : " << t.departureTime << "\n";
    cout << "Arrival    : " << t.arrivalTime   << "\n";
    cout << "Days       : " << t.runningDays   << "\n";
    cout << "Seats      : " << t.availableSeats << "/" << t.totalSeats << " available\n";
    cout << "Fare       : Rs." << t.fare        << "\n";
}

void displayTrainList(const vector<Train>& trains) {
    if (trains.empty()) {
        cout << "No trains found.\n";
        return;
    }
    for (const Train& t : trains) displayTrain(t);
    cout << "─────────────────────────────────────\n";
}

// ─────────────────────────────────────────────
// Simple Tests  (Closes #8)
// ─────────────────────────────────────────────

void runTests() {
    cout << "\n========== RUNNING TESTS ==========\n";
    int passed = 0, failed = 0;

    // Build a small test database
    vector<Train> db;

    Train t1 = {101, "Rajdhani Express", "Delhi", "Mumbai", "08:00", "20:00",
                "Mon,Wed,Fri", 500, 120, 1500.0f};
    Train t2 = {102, "Shatabdi Express", "Delhi", "Mumbai", "06:00", "14:00",
                "Mon,Tue,Wed,Thu,Fri,Sat,Sun", 300, 0, 1200.0f};
    Train t3 = {103, "Garib Rath",       "Mumbai", "Delhi", "10:00", "22:00",
                "Tue,Thu,Sat", 400, 200, 800.0f};
    db.push_back(t1);
    db.push_back(t2);
    db.push_back(t3);

    // Test 1: search by source + destination
    {
        vector<Train> res = searchTrains(db, "Delhi", "Mumbai");
        bool ok = (res.size() == 2);
        cout << "[TEST 1] Search Delhi→Mumbai: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 2: search wrong route
    {
        vector<Train> res = searchTrains(db, "Delhi", "Chennai");
        bool ok = (res.empty());
        cout << "[TEST 2] Search Delhi→Chennai (no match): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 3: case-insensitive search
    {
        vector<Train> res = searchTrains(db, "delhi", "MUMBAI");
        bool ok = (res.size() == 2);
        cout << "[TEST 3] Case-insensitive search: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 4: search by day — Mon
    {
        vector<Train> res = searchTrainsByDay(db, "Delhi", "Mumbai", "Mon");
        bool ok = (res.size() == 2); // Both t1 and t2 run on Mon
        cout << "[TEST 4] Search Delhi→Mumbai on Mon: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 5: search by day — Wed filter
    {
        vector<Train> res = searchTrainsByDay(db, "Delhi", "Mumbai", "Wed");
        bool ok = (res.size() == 2);
        cout << "[TEST 5] Search Delhi→Mumbai on Wed: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 6: validation — same source/dest
    {
        Train bad = {104, "Test", "Delhi", "Delhi", "08:00", "20:00", "Mon", 100, 50, 500.0f};
        bool ok = !isValidTrain(bad);
        cout << "[TEST 6] Reject same src/dst: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 7: validation — invalid time
    {
        Train bad = {105, "Test", "Delhi", "Mumbai", "25:00", "20:00", "Mon", 100, 50, 500.0f};
        bool ok = !isValidTrain(bad);
        cout << "[TEST 7] Reject invalid departure time: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 8: validation — negative fare
    {
        Train bad = {106, "Test", "Delhi", "Mumbai", "08:00", "20:00", "Mon", 100, 50, -100.0f};
        bool ok = !isValidTrain(bad);
        cout << "[TEST 8] Reject negative fare: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    cout << "====================================\n";
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

int main() {
    cout << "Train data structure created.\n";
    runTests();
    return 0;
}
