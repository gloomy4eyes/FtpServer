#pragma once

#include "FileTransport.h"
#include <Poco/Net/FTPClientSession.h>
#include <Poco/Path.h>
#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>

inline std::unique_ptr<Poco::Net::FTPClientSession> getNewFtpSession(const std::string& ftpConnectionString, std::string& sessionId) {
  Poco::StringTokenizer st(ftpConnectionString, "@", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
  Poco::StringTokenizer stlp(st[0], ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
  Poco::StringTokenizer stip(st[1], ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
  sessionId = stlp[0];
  std::unique_ptr<Poco::Net::FTPClientSession> ftpClientSessionPtr(new Poco::Net::FTPClientSession(stip[0], static_cast<Poco::UInt16>(std::stoi(stip[1])), stlp[0], stlp[1]));
  if (!ftpClientSessionPtr->isOpen()) {
    throw std::runtime_error("session is not open");
  }
  if (!ftpClientSessionPtr->isLoggedIn()) {
    throw std::runtime_error("user is not logged in");
  }
  return ftpClientSessionPtr;
}

namespace swf {
namespace transport {
class FileTransportFtp : public FileTransport {
  std::string _sessionId;
  std::unique_ptr<Poco::Net::FTPClientSession> _ftpClientSessionPtr;

public:
  explicit FileTransportFtp(const std::string & ftpConnectionString) : _ftpClientSessionPtr(getNewFtpSession(ftpConnectionString, _sessionId)) {}

  void upload(const std::string& fileId, const std::string& absoluteFilePath) override {
    Poco::Path path;
    if (!path.tryParse(absoluteFilePath)) {
      throw std::runtime_error("invalid file path");
    }
    Poco::FileInputStream fis(path.toString());
    Poco::StreamCopier::copyStream(fis, _ftpClientSessionPtr->beginUpload(fileId));
    _ftpClientSessionPtr->endUpload();
  }
  void download(const std::string& fileId, std::ostream& ostream) override { 
    Poco::StreamCopier::copyStream(_ftpClientSessionPtr->beginDownload(fileId), ostream);
    _ftpClientSessionPtr->endDownload();
  }
  void download(const std::string& fileId, const char* data, size_t& size) override {
    
  }

  void upload(const std::string& fileId, const char* data, size_t size) override { 
    auto& istr = _ftpClientSessionPtr->beginUpload(fileId);
    istr.write(data, size);
    _ftpClientSessionPtr->endUpload();
  }
  void download(const std::string& fileId, const std::string& absoluteFilePath) override;
};
}
}
