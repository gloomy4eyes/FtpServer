#ifndef PG_DB_POOL_H_INCLUDED
#define PG_DB_POOL_H_INCLUDED

#include "PgDbConnection.h"
#include "BlockingConcurrentQueue.hpp"

using TD_connQ = moodycamel::BlockingConcurrentQueue<std::shared_ptr<pqxx::connection>>;

class PgDbPool {
  TD_connQ _connections;

 public:
  PgDbPool(const std::string& connectionString, uint8_t poolCount);
  ~PgDbPool();
  PgDbConnection getConnection();
  TD_connQ& connQ();
//  std::atomic<uint32_t> qc{0};
};

#endif  // PG_DB_POOL_H_INCLUDED
