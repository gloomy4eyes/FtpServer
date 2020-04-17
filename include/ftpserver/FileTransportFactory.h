#pragma once

#include "FileTransport.h"
#include "FileTransportFtp.h"

namespace swf {
namespace transport {
class FileTransportFactory {
public:

  static FileTransport* createFileTransport(const std::string& ftpConnectionString) { return new FileTransportFtp(ftpConnectionString); }
};
}
}
