#pragma once

#include "StorageClientRpc.h"
#include "StorageClientLocal.h"

namespace swf {
class StorageClientFactory {
 public:
  StorageClientFactory() {}

  static StorageClient* createStorageClient(jsonrpc::JMSClient& jmsClient, const std::string& sessionId) { 
    return new StorageClientRpc(jmsClient, sessionId);
  }

  static StorageClient* createStorageClient(const std::string& storageConnectionString) { 
    return new StorageClientLocal(storageConnectionString);
  }
};
}  // namespace swf
