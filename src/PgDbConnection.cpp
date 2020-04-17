#include "PgDbConnection.h"
#include "PgDbPool.h"

PgDbConnection::PgDbConnection(std::shared_ptr<pqxx::connection> pgConnection, PgDbPool* parent) : _pgConnection(std::move(pgConnection)), _connQ(parent) {}

PgDbConnection::~PgDbConnection() { _connQ->connQ().enqueue(std::move(_pgConnection)); }

PgDbConnection::PgDbConnection(PgDbConnection&& pgDbConnection) noexcept : _pgConnection(std::move(pgDbConnection._pgConnection)), _connQ(pgDbConnection._connQ) {}

pqxx::connection* PgDbConnection::getConnection() const { return _pgConnection.get(); }
