#pragma once

#include "StorageClient.h"
#include "Communication.h"
#include "JmsTransportClient.h"
#include "../storage/storageclientfunctions.h"

#include "FileObject.h"

namespace swf {
class StorageClientRpc : public StorageClient {
  jsonrpc::JMSClient& _jmsClient;
  StorageClientFunctions _storage;
  std::string _sessionId;
 public:
  StorageClientRpc(jsonrpc::JMSClient & jmsClient, const std::string& sessionId) : _jmsClient(jmsClient), _storage(jmsClient), _sessionId(sessionId) {}

  void createFile(archive::object::File& file) override {
    auto content(file.content());
    createFile(file, content);
  }

  void createFile(archive::object::File& file, jsoncons::json& content) override {
    auto answer = _storage.create_object((content), file.base.id(), _sessionId, "", "", file.type());
    if (answer["result"].asInt() != 0) {
      throw std::runtime_error("error on creation file entry");
    }
  }

  bool isFileExists(const std::string& fileId) override { 
    auto answer = _storage.get_object(fileId, _sessionId);
    return answer["result"].asInt() == 0;
  }
};
}
