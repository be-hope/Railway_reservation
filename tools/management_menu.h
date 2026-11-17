#pragma once
#include "admin/train_admin_service.h"
#include "admin/report_service.h"
#include "ui/console_io.h"
#include "admin/reservation_admin_service.h"
#include "cancel/cancellation_service.h"
namespace rrs {
void runManagementMenu(ConsoleIO&, const Session&, const DataAccess&, const IClock&, IAuditLogger&, IBookingService&, CancellationService&);
}
