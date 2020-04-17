#include "PgDbPool.h"
#include <Poco/Logger.h>

TD_connQ _initConnections_(const std::string& connectionString, const uint8_t poolCount) {
  TD_connQ conns;
  for (auto cnt = 0; cnt < poolCount; ++cnt) {
    if (!conns.enqueue(std::make_shared<pqxx::connection>(connectionString))) {
      throw std::runtime_error("create db pool fail");
    }
  }
  return conns;
}

PgDbPool::PgDbPool(const std::string& connectionString, const uint8_t poolCount) : _connections(_initConnections_(connectionString, poolCount)) /*, qc(poolCount)*/ {}

PgDbPool::~PgDbPool() {}

PgDbConnection PgDbPool::getConnection() {
  std::shared_ptr<pqxx::connection> connPtr;
  _connections.wait_dequeue(connPtr);
  // TODO: quiting check
  if (connPtr == nullptr) {
    throw std::runtime_error("cannot access to connection");
  }
  return PgDbConnection(std::move(connPtr), this);
}

TD_connQ& PgDbPool::connQ() { return _connections; }
