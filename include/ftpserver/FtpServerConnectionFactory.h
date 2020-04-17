#ifndef FTP_SERVER_CONNECTION_FACTORY_H_INCLUDED
#define FTP_SERVER_CONNECTION_FACTORY_H_INCLUDED

#include <Poco/Net/TCPServerConnectionFactory.h>
#include <BlockingConcurrentQueue.hpp>
#include "FtpServerConnection.h"

class FtpServerConnectionFactory : public Poco::Net::TCPServerConnectionFactory {
 public:
  FtpServerConnectionFactory(PgDbPool& pgDbPool, const std::string& connectionString, moodycamel::BlockingConcurrentQueue<uint16_t>& ports, uint32_t contextPoolCount, Poco::Timespan& timeOut)
      : _pgDbPool(pgDbPool), _ports(ports), _timeOut(timeOut) {}
  ~FtpServerConnectionFactory() override = default;

  FtpServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override { return new FtpServerConnection(socket, _pgDbPool, _ports, _timeOut); }

  FtpServerConnectionFactory(const FtpServerConnectionFactory&) = delete;
  FtpServerConnectionFactory& operator=(const FtpServerConnectionFactory&) = delete;
  FtpServerConnectionFactory(FtpServerConnectionFactory&&) noexcept = delete;
  FtpServerConnectionFactory& operator=(FtpServerConnectionFactory&&) noexcept = delete;

 private:
  PgDbPool& _pgDbPool;
  moodycamel::BlockingConcurrentQueue<uint16_t>& _ports;
  Poco::Timespan _timeOut;
};

#endif  // FTP_SERVER_CONNECTION_FACTORY_H_INCLUDED
