#pragma once

#include "StorageClient.h"
#include "StorageContextFactory.h"
#include "../libengine/repository/queries/CreateObject.h"
#include "../libengine/repository/queries/SimpleFind.h"

namespace swf {
class StorageClientLocal : public StorageClient {
  repository::engine::ContextFactory& _contextFactory;

public:
  void createFile(archive::object::File& file, jsoncons::json& content) override {
   _contextFactory.create("");
   repository::queries::CreateObject::exec(file.base.id(), file.type(), content);
   _contextFactory.commit(swf::repository::Error::Success);
  }

  explicit StorageClientLocal(const std::string& storageConnectionString) : _contextFactory(swf::repository::engine::ContextFactory::getInstance()) { 
    _contextFactory.init(storageConnectionString, 10);
  }

  void createFile(archive::object::File& file) override {
    auto content(file.content());
    createFile(file, content);
  }

  bool isFileExists(const std::string& fileId) override {
    _contextFactory.create("");
    auto fvec = repository::queries::SimpleFind::exec<std::string>(FIELD_AND(FIELD_EQUAL_TYPE(archive::object::file::option::file), FIELD_EQUAL_STRING(FIELD_BASE("id"), fileId)));
    _contextFactory.commit(swf::repository::Error::Success);
    return !fvec.empty();
  }
};
}
