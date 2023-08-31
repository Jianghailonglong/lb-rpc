#pragma once

#include "mysql.h"
#include "lbrss.pb.h"

class StoreReport
{
public:
    StoreReport();

    void store(lbrss::ReportStatusRequest req);

private:
    MYSQL db_conn_;
};