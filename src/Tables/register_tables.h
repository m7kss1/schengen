#pragma once

#include "Tables/customer.h"
#include "Tables/lineitem.h"
#include "Tables/nation.h"
#include "Tables/orders.h"
#include "Tables/part.h"
#include "Tables/partsupp.h"
#include "Tables/region.h"
#include "Tables/supplier.h"
#include "Common/registry.h"

void RegisterTableCustomer(TableRegistry & registry);
void RegisterTableNation(TableRegistry & registry);
void RegisterTableRegion(TableRegistry & registry);
void RegisterTableSupplier(TableRegistry & registry);
void RegisterTablePartSupp(TableRegistry & registry);
void RegisterTablePart(TableRegistry & registry);
void RegisterTableOrders(TableRegistry & registry);
void RegisterTableLineitem(TableRegistry & registry);
void RegisterTables();
