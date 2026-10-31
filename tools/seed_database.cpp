#include <iostream>
#include <string>

#include "common/clock.h"
#include "common/date.h"
#include "common/result.h"
#include "dal/sqlite_repositories.h"
#include "seed_data.h"

int main(int argc, char** argv) {
    using namespace rrs;

    if (argc > 3) {
        std::cerr << "Usage: rrs_seed_database [database-file] [YYYY-MM-DD]\n";
        return 1;
    }

    const std::string databasePath =
        argc >= 2 ? argv[1] : "rrs_seed.db";

    SystemClock clock;
    Date seedDate = clock.today();

    if (argc == 3) {
        auto parsedDate = Date::parseIso(argv[2]);

        if (!parsedDate) {
            std::cerr << "Invalid date. Use YYYY-MM-DD.\n";
            return 1;
        }

        seedDate = *parsedDate;
    }

    auto store = SqliteDataStore::open(databasePath);

    if (!store.ok()) {
        std::cerr << formatError(store.error()) << '\n';
        return 1;
    }

    DataAccess data = store.value()->access();

    Status result = seed::loadDemoSeedData(data, seedDate);

    if (!result.ok()) {
        std::cerr << formatError(result.error()) << '\n';
        return 1;
    }

    std::cout << "Seed data loaded successfully.\n"
              << "Database: " << databasePath << '\n'
              << "Seed date: " << seedDate.iso() << '\n'
              << "Stations: 5\n"
              << "Trains: 4\n"
              << "Test accounts: 5\n"
              << "Sample reservations: 4\n"
              << "\nTest account login IDs:\n"
              << "Passenger 1: 9876500001\n"
              << "Passenger 2: 9876500002\n"
              << "Staff: staff01\n"
              << "Admin: admin01\n"
              << "Operations: ops01\n"
              << "\nNote: These accounts currently have disabled password hashes.\n";

    return 0;
}
