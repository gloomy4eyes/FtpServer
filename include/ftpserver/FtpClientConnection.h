#ifndef FTP_CLIENT_CONNECTION_H_INCLUDED
#define FTP_CLIENT_CONNECTION_H_INCLUDED

#include <string>
#include <map>
#include <memory>

class StorageClientFunctions;

namespace jsonrpc {
class JMSClient;
}

namespace Poco {
class File;
class Path;
namespace Net {
class FTPClientSession;
class StreamSocket;
}  // namespace Net
}  // namespace Poco

namespace swf {
namespace transport {
class FileTransport;
}

class StorageClient;
class Communication;
namespace repository {
namespace engine {
class ContextFactory;
}
}  // namespace repository
namespace ftp {
using UserDefinedMeta = std::map<std::string, std::string>;

struct BlobData {
  char* data;
  size_t size;

  bool isValid() const { return data != nullptr; }
};

class FtpClientConnection {
  std::string _sessionId;
  StorageClient& _sc;
  transport::FileTransport& _ft;

  void upload(const std::string& fileName, const std::string& filePath) const;
  void upload(const std::string& fileName, std::unique_ptr<BlobData> blobDataPtr) const;

 public:
  FtpClientConnection(StorageClient& sc, transport::FileTransport& ft);
  FtpClientConnection(const FtpClientConnection& rhs) = delete;
  FtpClientConnection(FtpClientConnection&& rhs) = default;
  ~FtpClientConnection();

  FtpClientConnection& operator=(const FtpClientConnection& rhs) = delete;
  FtpClientConnection& operator=(FtpClientConnection&& rhs) = delete;

  std::string upload(const std::string& absoluteFilePath) const;
  bool isEntryExists(const std::string& fileId) const;
  void upload(const std::string& fileId, std::unique_ptr<BlobData> blobDataPtr);
  std::string upload(const std::string& fileId, std::istream& istr);
  std::string upload(const std::string& absoluteFilePath, UserDefinedMeta& meta) const;

  void download(const std::string& fileId, const std::string& absoluteDestinationPath) const;
  void download(const std::string& fileId, std::ostream& ostr) const;
};
}  // namespace ftp
}  // namespace swf

#endif  // FTP_CLIENT_CONNECTION_H_INCLUDED
