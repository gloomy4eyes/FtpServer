#pragma once

#include <string>
#include "FileObject.h"

namespace swf {
class StorageClient {
public:
  StorageClient() {}
  virtual ~StorageClient() {}

  virtual void createFile(archive::object::File & file) = 0;
  virtual void createFile(archive::object::File& file, jsoncons::json& content) = 0;

  virtual bool isFileExists(const std::string& fileId) = 0;
};
}
