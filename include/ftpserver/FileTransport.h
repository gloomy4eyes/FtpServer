#pragma once

#include <string>

namespace swf {

  class FileBlob;
namespace transport {


class FileTransport {
public:
  FileTransport() = default;
  virtual ~FileTransport() = default;
  virtual void upload(const std::string& fileId, const std::string& absoluteFilePath) = 0;
  virtual void upload(const std::string& fileId, const char* data, size_t size) = 0;

  virtual void download(const std::string& fileId, const std::string& absoluteFilePath) = 0;
  virtual void download(const std::string& fileId, std::ostream& ostream) = 0;
  virtual void download(const std::string& fileId, const char* data, size_t& size) = 0;

protected:
  
};
}
}
