// route.cpp
// Sprint 1 - Route & Schedule Management
// Closes #13 (Add/view routes)
// Closes #14 (Update/delete routes)
// Closes #15 (Manage schedules and fares)

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

// ─────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────

struct Stop {
    string stationName;
    string arrivalTime;    // "HH:MM" or "START"
    string departureTime;  // "HH:MM" or "END"
    int    distanceFromOrigin; // km
};

struct Route {
    int         routeId;
    string      routeName;
    vector<Stop> stops;       // ordered list of intermediate stops
};

struct Schedule {
    int    scheduleId;
    int    routeId;
    int    trainId;
    string day;              // e.g. "Mon"
    float  baseFare;         // Rs.
    float  farePerKm;        // Rs./km
};

// ─────────────────────────────────────────────
// Route CRUD
// ─────────────────────────────────────────────

bool addRoute(vector<Route>& routes, const Route& r) {
    if (r.routeId <= 0 || r.routeName.empty() || r.stops.size() < 2) {
        cout << "Error: invalid route data (need routeId>0, name, ≥2 stops).\n";
        return false;
    }
    for (const Route& existing : routes) {
        if (existing.routeId == r.routeId) {
            cout << "Error: routeId " << r.routeId << " already exists.\n";
            return false;
        }
    }
    routes.push_back(r);
    cout << "Route " << r.routeId << " (" << r.routeName << ") added.\n";
    return true;
}

bool deleteRoute(vector<Route>& routes, int routeId) {
    for (auto it = routes.begin(); it != routes.end(); ++it) {
        if (it->routeId == routeId) {
            cout << "Route " << routeId << " (" << it->routeName << ") deleted.\n";
            routes.erase(it);
            return true;
        }
    }
    cout << "Error: routeId " << routeId << " not found.\n";
    return false;
}

Route* findRoute(vector<Route>& routes, int routeId) {
    for (Route& r : routes) {
        if (r.routeId == routeId) return &r;
    }
    return nullptr;
}

bool updateRouteName(vector<Route>& routes, int routeId, const string& newName) {
    Route* r = findRoute(routes, routeId);
    if (!r) { cout << "Error: routeId " << routeId << " not found.\n"; return false; }
    r->routeName = newName;
    cout << "Route " << routeId << " renamed to '" << newName << "'.\n";
    return true;
}

void displayRoute(const Route& r) {
    cout << "─────────────────────────────────────\n";
    cout << "Route ID : " << r.routeId << "  Name: " << r.routeName << "\n";
    cout << "Stops (" << r.stops.size() << "):\n";
    for (size_t i = 0; i < r.stops.size(); i++) {
        const Stop& s = r.stops[i];
        cout << "  [" << i+1 << "] " << s.stationName
             << "  arr:" << s.arrivalTime
             << "  dep:" << s.departureTime
             << "  dist:" << s.distanceFromOrigin << "km\n";
    }
}

// ─────────────────────────────────────────────
// Schedule CRUD
// ─────────────────────────────────────────────

bool addSchedule(vector<Schedule>& schedules, const Schedule& sc) {
    if (sc.scheduleId <= 0 || sc.routeId <= 0 || sc.trainId <= 0 || sc.day.empty()) {
        cout << "Error: invalid schedule data.\n";
        return false;
    }
    if (sc.baseFare < 0 || sc.farePerKm < 0) {
        cout << "Error: fare values cannot be negative.\n";
        return false;
    }
    for (const Schedule& existing : schedules) {
        if (existing.scheduleId == sc.scheduleId) {
            cout << "Error: scheduleId " << sc.scheduleId << " already exists.\n";
            return false;
        }
    }
    schedules.push_back(sc);
    cout << "Schedule " << sc.scheduleId << " added (Train " << sc.trainId
         << " on Route " << sc.routeId << " every " << sc.day << ").\n";
    return true;
}

bool deleteSchedule(vector<Schedule>& schedules, int scheduleId) {
    for (auto it = schedules.begin(); it != schedules.end(); ++it) {
        if (it->scheduleId == scheduleId) {
            schedules.erase(it);
            cout << "Schedule " << scheduleId << " deleted.\n";
            return true;
        }
    }
    cout << "Error: scheduleId " << scheduleId << " not found.\n";
    return false;
}

bool updateFare(vector<Schedule>& schedules, int scheduleId,
                float newBaseFare, float newFarePerKm) {
    if (newBaseFare < 0 || newFarePerKm < 0) {
        cout << "Error: fare values cannot be negative.\n";
        return false;
    }
    for (Schedule& sc : schedules) {
        if (sc.scheduleId == scheduleId) {
            sc.baseFare   = newBaseFare;
            sc.farePerKm  = newFarePerKm;
            cout << "Schedule " << scheduleId << " fare updated: base=Rs."
                 << newBaseFare << " perKm=Rs." << newFarePerKm << "\n";
            return true;
        }
    }
    cout << "Error: scheduleId " << scheduleId << " not found.\n";
    return false;
}

float calculateFare(const Schedule& sc, int distanceKm) {
    return sc.baseFare + (sc.farePerKm * distanceKm);
}

void displaySchedule(const Schedule& sc) {
    cout << "─────────────────────────────────────\n";
    cout << "Schedule ID : " << sc.scheduleId  << "\n";
    cout << "Train ID    : " << sc.trainId     << "\n";
    cout << "Route ID    : " << sc.routeId     << "\n";
    cout << "Day         : " << sc.day         << "\n";
    cout << "Base Fare   : Rs." << sc.baseFare << "\n";
    cout << "Per Km      : Rs." << sc.farePerKm << "\n";
}

// ─────────────────────────────────────────────
// Simple Tests
// ─────────────────────────────────────────────

void runRouteTests() {
    cout << "\n========== ROUTE & SCHEDULE TESTS ==========\n";
    int passed = 0, failed = 0;

    vector<Route>   routes;
    vector<Schedule> schedules;

    Stop s1 = {"Delhi",       "START", "08:00", 0};
    Stop s2 = {"Mathura",     "09:30", "09:35", 140};
    Stop s3 = {"Agra",        "10:15", "10:20", 200};
    Stop s4 = {"Mumbai",      "20:00", "END",   1400};

    Route r1 = {1, "Delhi-Mumbai Express", {s1, s2, s3, s4}};

    // Test 1: Add route
    {
        bool ok = addRoute(routes, r1);
        cout << "[TEST 1] Add route: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 2: Duplicate routeId
    {
        Route dup = {1, "Duplicate", {s1, s4}};
        bool ok = !addRoute(routes, dup);
        cout << "[TEST 2] Reject duplicate routeId: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 3: Too few stops
    {
        Route bad = {2, "OneStop", {s1}};
        bool ok = !addRoute(routes, bad);
        cout << "[TEST 3] Reject route with <2 stops: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 4: Rename route
    {
        bool ok = updateRouteName(routes, 1, "Rajdhani Route");
        cout << "[TEST 4] Rename route: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 5: Add schedule
    {
        Schedule sc = {10, 1, 101, "Mon", 200.0f, 1.5f};
        bool ok = addSchedule(schedules, sc);
        cout << "[TEST 5] Add schedule: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 6: Update fare
    {
        bool ok = updateFare(schedules, 10, 250.0f, 2.0f);
        cout << "[TEST 6] Update fare: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 7: Calculate fare
    {
        Schedule& sc = schedules[0];
        float fare = calculateFare(sc, 1400); // Delhi→Mumbai ~1400km
        bool ok = (fare == 250.0f + 2.0f * 1400);
        cout << "[TEST 7] Calc fare Rs." << fare << " (expect Rs.3050): " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 8: Delete schedule
    {
        bool ok = deleteSchedule(schedules, 10);
        cout << "[TEST 8] Delete schedule: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    // Test 9: Delete route
    {
        bool ok = deleteRoute(routes, 1);
        cout << "[TEST 9] Delete route: " << (ok ? "PASS" : "FAIL") << "\n";
        ok ? passed++ : failed++;
    }

    cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    cout << "=============================================\n";
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

int main() {
    cout << "Route and Schedule management module loaded.\n";
    runRouteTests();
    return 0;
}
