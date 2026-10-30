#include "dal/retention.h"
namespace rrs {
Result<int> clearExpiredTravellerIds(SqliteDatabase& db, const Date& today) {
    SqliteUnitOfWork uow(db);
    TransactionGuard tx(uow); if (!tx.started().ok()) return tx.started().error();
    Statement statement(db, "UPDATE traveller SET id_proof_number = NULL WHERE id_proof_number IS NOT NULL AND pnr IN (SELECT pnr FROM reservation WHERE journey_date < ?)");
    auto status = statement.bind(1, today.plusDays(-90).iso()).execute();
    if (!status.ok()) return status.error();
    const int count = db.changes();
    status = tx.commit(); if (!status.ok()) return status.error();
    return count;
}
}
