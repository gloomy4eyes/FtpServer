#include "FtpClientConnection.h"
#include <Poco/Net/FTPClientSession.h>
#include <Poco/Path.h>
#include <Poco/File.h>
// #include "StorageClient.h"
#include "FileTransport.h"
// #include "jsoncons/json.hpp"

// swf::archive::object::File generateFileObject(const Poco::Path& path, const Poco::File& file) {
//   swf::archive::object::File fileObject;
//   fileObject.setName(path.getFileName());
//   fileObject.setSize(file.getSize());
//   fileObject.setExtension(path.getExtension());
//   fileObject.setType("file");
//   fileObject.setClassification("doka_grif_ns");
//   return fileObject;
// }

// void swf::ftp::FtpClientConnection::upload(const std::string& fileName, std::unique_ptr<swf::ftp::BlobData> blobDataPtr) const {
//  // if (blobDataPtr->isValid()) {
//  //   auto& uploadStream = _ftpClientSessionPtr->beginUpload(fileName);
//  //   uploadStream.write(blobDataPtr->data, blobDataPtr->size);
//  //   if (uploadStream.bad() && !uploadStream.good()) {
//  //     throw std::runtime_error("error on upload data");
//  //   }
//  //   _ftpClientSessionPtr->endUpload();
//  // }
//}

swf::ftp::FtpClientConnection::~FtpClientConnection() {}

swf::ftp::FtpClientConnection::FtpClientConnection(StorageClient& sc, transport::FileTransport& ft) : _sc(sc), _ft(ft) {}

std::string swf::ftp::FtpClientConnection::upload(const std::string& absoluteFilePath) const {
  Poco::Path path;
  if (!path.tryParse(absoluteFilePath)) {
    throw std::runtime_error("invalid file path");
  }
  Poco::File file(path);
  if (!file.isFile()) {
    throw std::runtime_error("... is not file");
  }

  // auto fileObject = generateFileObject(path, file);
  //
  // _sc.createFile(fileObject);
  // _ft.upload(fileObject.base.id(), absoluteFilePath);

  // return fileObject.base.id();
  return {};
}

bool swf::ftp::FtpClientConnection::isEntryExists(const std::string& fileId) const { return false /*_sc.isFileExists(fileId)*/; }

void swf::ftp::FtpClientConnection::upload(const std::string& fileId, std::unique_ptr<BlobData> blobDataPtr) {
  if (!isEntryExists(fileId)) {
    throw std::runtime_error("entry not exists");
  }
}

std::string swf::ftp::FtpClientConnection::upload(const std::string& absoluteFilePath, UserDefinedMeta& meta) const {
  Poco::Path path;
  if (!path.tryParse(absoluteFilePath)) {
    throw std::runtime_error("invalid file path");
  }
  Poco::File file(path);
  if (!file.isFile()) {
    throw std::runtime_error("... is not file");
  }

  // auto fileObject = generateFileObject(path, file);
  // auto content(fileObject.content());
  // content.add(jsoncons::json(meta));
  //
  // _sc.createFile(fileObject, content);
  // _ft.upload(fileObject.base.id(), path.toString());
  //
  // return fileObject.base.id();
  return {};
}

void swf::ftp::FtpClientConnection::download(const std::string& fileId, const std::string& absoluteDestinationPath) const { _ft.download(fileId, absoluteDestinationPath); }

void swf::ftp::FtpClientConnection::download(const std::string& fileId, std::basic_ostream<char>& ostr) const { _ft.download(fileId, ostr); }
