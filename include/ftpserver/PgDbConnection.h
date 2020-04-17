#ifndef PG_DBCONNECTION_H_INCLUDED
#define PG_DBCONNECTION_H_INCLUDED

#include <pqxx/connection>

class PgDbPool;

class PgDbConnection {
 public:
  PgDbConnection(std::shared_ptr<pqxx::connection> pgConnection, PgDbPool* parent);
  ~PgDbConnection();
  PgDbConnection(PgDbConnection&& pgDbConnection) noexcept;
  pqxx::connection* getConnection() const;

 private:
  std::shared_ptr<pqxx::connection> _pgConnection;
  PgDbPool* _connQ;
};

#endif  // PG_DBCONNECTION_H_INCLUDED
