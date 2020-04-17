#include "ftpserver/FtpServerConnection.h"

#include <Poco/LocalDateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Path.h>
#include <Poco/String.h>
#include <Poco/Hash.h>
#include <Poco/Error.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Logger.h>
#include <Poco/Net/NetException.h>
#include <Poco/RWLock.h>
#include <Poco/LogStream.h>

#include "ftpserver/PgDbPool.h"
//#include <pqxx/largeobject.hxx>
#include <Poco/UUIDGenerator.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

void FtpServerConnection::sendData(std::string bytesBuffer, Poco::Net::StreamSocket& socket) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  int lastError = 0;
  int sent = 0;
  int all = static_cast<int>(bytesBuffer.size());
  do {
    errno = 0;
    lastError = 0;
    try {
      sent = socket.sendBytes(&bytesBuffer[sent], all, MSG_NOSIGNAL);
      if (sent < 1) {
        logger.error(Poco::format("error on send data to %d, error: %d", static_cast<int>(socket.impl()->sockfd()), socket.impl()->socketError()));
      } else {
        all -= sent;
      }
    } catch (Poco::Net::NetException& exc) {
      lastError = exc.code();
      logger.log(exc);
    } catch (Poco::Exception& exc) {
      lastError = exc.code();
      logger.log(exc);
    } catch (std::exception& err) {
      logger.log(Poco::format("send Exception: %s", err.what()));
    }
  } while ((Poco::Error::last() == POCO_EAGAIN || lastError == POCO_EAGAIN) || (all > 0 && sent > 0));
}

void FtpServerConnection::sendReply(std::string message) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  try {
    message.append("\r\n");
    logger.information(Poco::format("%d A: %s", static_cast<int>(socket().impl()->sockfd()), message));
    sendData(std::move(message), socket());
  } catch (Poco::Exception& exc) {
    logger.log(exc);
  } catch (std::exception& err) {
    logger.error("send exception: %s", err.what());
  }
}

bool FtpServerConnection::openDataConnection() {
  if (_eStatus != FtpServerStatus::WAITING) {
    sendReply("425 You're already connected.");
    return false;
  }
  if (_eDataConnection == Dataconnection::PASV) {
    if (_dataSock->impl()->sockfd() != POCO_INVALID_SOCKET) {
      _outSock = _dataSock->acceptConnection();
      if (_outSock.impl()->sockfd() != POCO_INVALID_SOCKET) {
        sendReply("150 Connection accepted.");
        return true;
      }
      sendReply("425 Can't open data connection.");
      closeDataConnection();
      return false;
    }
    sendReply("503 internal error");
    return false;
  }
  if (_eDataConnection == Dataconnection::PORT) {
    return _outSock.impl()->sockfd() != POCO_INVALID_SOCKET;
  }
  if (_eDataConnection == Dataconnection::NONE) {
    sendReply("503 Bad sequence of commands.");
    return false;
  }
  return false;
}

void FtpServerConnection::closeDataConnection() {
  if (_outSock.impl()->sockfd() != POCO_INVALID_SOCKET) {
    _outSock.close();
  }

  if (_dataSock != nullptr) {
    _ports.enqueue(_dataSock->address().port());
    if (_dataSock->impl()->sockfd() != POCO_INVALID_SOCKET) {
      _dataSock->close();
    }
  }
  _eStatus = FtpServerStatus::WAITING;
  _eDataConnection = Dataconnection::NONE;
}

void FtpServerConnection::retrieveData(const std::string& dataId) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  std::unique_ptr<pqxx::work> pw{nullptr};

  try {
    auto pgDbConnection = _pgDbPool.getConnection();
    pw = std::make_unique<pqxx::work>(*pgDbConnection.getConnection());
    const std::string storageSelectQuery{Poco::format("SELECT oid from bin_storage WHERE guid IN (SELECT internal_uid FROM storage WHERE external_uid = \'%s\');", dataId)};
    auto result = pw->exec(storageSelectQuery);
    if (result.empty()) {
      sendReply(Poco::format("550 File %s not found.", dataId));
      return;
    }
    pqxx::largeobjectaccess loa(*pw, result.begin()->begin().as<pqxx::oid>(), std::ios::in | std::ios::out);

    loa.seek(0, std::ios::end);
    const auto loaSize = loa.tell();
    loa.seek(static_cast<pqxx::largeobject::size_type>(_restartAt), std::ios::beg);

    if (_restartAt == 0 || (_restartAt > 0 && loa.tell() == std::streampos(_restartAt))) {
      if (!openDataConnection()) {
        return;
      }
      size_t bytesToTransfer = loaSize - _restartAt;
      _eStatus = FtpServerStatus::DOWNLOADING;
      pqxx::largeobjectaccess::size_type readed = 0;
      while (bytesToTransfer != 0) {
        readed = loa.read(&_buff[0], (static_cast<size_t>(bytesToTransfer) <= frameSize) ? static_cast<pqxx::largeobject::size_type>(bytesToTransfer) : frameSize);
        int lastError = 0;
        int len = 0;
        int all = static_cast<int>(readed);
        do {
          errno = 0;
          lastError = 0;
          try {
            len = _outSock.sendBytes(&_buff[len], all, MSG_NOSIGNAL);
            if (len > 0) {
              all -= len;
            }
          } catch (const Poco::Exception& exc) {
            lastError = exc.code();
            if (lastError != POCO_EAGAIN) {
              logger.log(exc);
              exc.rethrow();
            }
          }
        } while ((Poco::Error::last() == POCO_EAGAIN || lastError == POCO_EAGAIN) || (all > 0 && len > 0));
        bytesToTransfer -= (readed);
      }
      pw->commit();
      sendReply("226 Transfer complete.");
      closeDataConnection();
    }
    return;
  } catch (const Poco::Exception& exc) {
    logger.log(exc);
    sendReply("550 Can't retrieve File. " + exc.displayText());
  } catch (const std::exception& err) {
    logger.error(err.what());
    sendReply(std::string("550 Can't retrieve File. ") + err.what());
  }
  try {
    if (pw != nullptr) {
      pw->abort();
    }
  } catch (...) {
  }
  closeDataConnection();
}

std::vector<std::string> FtpServerConnection::getFileEntry(const std::string& fileName) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  std::vector<std::string> vec;
  // const std::unique_ptr<swf::repository::engine::Transaction> transaction = _saeStorage.createTransaction();
  try {
    Poco::Path pp(fileName);
    // vec = swf::repository::queries::SimpleFind::exec<std::string>(swf::ID_AND_TYPE(pp.getBaseName(), swf::archive::classifier::file_type::file));
  } catch (Poco::Exception& exc) {
    logger.log(exc);
    // _isTransactError = true;
  } catch (std::exception& exc) {
    logger.error(exc.what());
    // _isTransactError = true;
  }

  //_saeStorage.deleteTransaction(transaction, error);
  return vec;
}

void FtpServerConnection::deleteFile(const std::string& fileName) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");

  try {
    auto pgDbConnection = _pgDbPool.getConnection();
    pqxx::work w(*pgDbConnection.getConnection());
    const std::string sqlQuery(Poco::format("SELECT oid from bin_storage WHERE guid IN (SELECT internal_uid FROM storage WHERE external_uid = \'%s\');", fileName));
    auto result = w.exec(sqlQuery);
    if (result.empty()) {
      sendReply(Poco::format("550 File %s not found", fileName));
      return;
    }
    std::string soid = result.begin()->begin()->c_str();
    pqxx::largeobjectaccess loa(w, static_cast<pqxx::oid>(std::stoi(soid)), std::ios::in | std::ios::out);
    loa.remove(w);

    w.commit();
    sendReply(Poco::format("250 file %s deleted succesfully", fileName));
  } catch (Poco::Exception& exc) {
    logger.log(exc);
    sendReply(Poco::format("550 Can't delete file %s. error: %s", fileName, exc.displayText()));
  } catch (std::exception& exc) {
    logger.error(exc.what());
    sendReply(Poco::format("550 Can't delete file %s. error: %s", fileName, exc.what()));
  }
}

bool FtpServerConnection::writeFileToBase(const std::string& cmd, pqxx::work& w, OidInBase& oib) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");

  pqxx::largeobjectaccess loa(w);
  loa.seek(0, std::ios::end);
  auto oidSize = static_cast<size_t>(loa.tell());
  _restartAt = (cmd == "APPE") ? loa.tell() : 0;

  // Смещаемся на размер полученного, или сначала
  loa.seek(_restartAt, std::ios::beg);
  std::streamsize totalTransferedBytes{0};
  if ((_restartAt > 0 && loa.tell() == std::streampos(_restartAt)) || _restartAt == 0) {
    if (!openDataConnection()) {
      return false;
    }
    std::vector<char> buff(frameSize);
    std::streamsize len{0};
    _eStatus = FtpServerStatus::UPLOADING;
    do {
      try {
        len = _outSock.receiveBytes(buff.data(), static_cast<int>(buff.size()));
        if (len > 0) {
          totalTransferedBytes += len;
          loa.write(buff.data(), static_cast<pqxx::largeobject::size_type>(len));
        }
      } catch (Poco::Exception& err) {
        logger.log(err);
        break;
      } catch (std::exception& err) {
        logger.error(err.what());
        break;
      }
    } while (len > 0);
  }
  if (totalTransferedBytes < 1) {
    // Удаляем файл
    w.abort();
    sendReply("550 Can't store file. Transfered bytest are less than 1");
    closeDataConnection();
    return false;
  }
  oidSize += static_cast<size_t>(totalTransferedBytes);
  oib.oid = loa.id();
  oib.size = oidSize;
  return true;
}

void FtpServerConnection::storFile(const std::string& cmd, const std::string& fileName) {
  auto& logger = Poco::Logger::get("docstorage");

  // std::string errStr{"550 Can't store file. error: "};
  logger.trace(Poco::format("%d, on storFile", static_cast<int>(socket().impl()->sockfd())));
  try {
    auto conn = _pgDbPool.getConnection();
    pqxx::work w(*conn.getConnection());

    OidInBase oib{};
    std::string excString;
    logger.trace(Poco::format("%d, before creating oid", static_cast<int>(socket().impl()->sockfd())));
    if (writeFileToBase(cmd, w, oib)) {
      try {
        logger.trace(Poco::format("%d, before inserts", static_cast<int>(socket().impl()->sockfd())));
        std::string entryUid{Poco::UUIDGenerator::defaultGenerator().createRandom().toString()};
        const std::string entryInsertQuery(Poco::format("INSERT INTO entry VALUES (\'%s\', %Lu);", entryUid, oib.size));
        w.exec(entryInsertQuery);
        const std::string bstorageInsertQuery(Poco::format("INSERT INTO bin_storage VALUES (\'%s\', %Lu);", entryUid, oib.oid));
        w.exec(bstorageInsertQuery);
        const std::string storageInsertQuery(Poco::format("INSERT INTO storage VALUES (\'%s\', \'%s\');", entryUid, fileName));
        w.exec(storageInsertQuery);
      } catch (const pqxx::pqxx_exception& ex) {
        excString = ex.base().what();
      } catch (const Poco::Exception& ex) {
        excString = ex.displayText();
      } catch (const std::runtime_error& ex) {
        excString = ex.what();
      }
      if (!excString.empty()) {
        w.abort();
        sendReply(Poco::format("550 %s", excString));
        closeDataConnection();
        return;
      }
      // logger.information("%s record updated", fileName);
      logger.trace(Poco::format("%d, record created, before commit", static_cast<int>(socket().impl()->sockfd())));
      w.commit();
      logger.trace(Poco::format("%d, after commit", static_cast<int>(socket().impl()->sockfd())));
      closeDataConnection();
      sendReply("226 Transfer complete.");
    }
  } catch (const pqxx::pqxx_exception& err) {
    logger.error(err.base().what());
  } catch (const Poco::Exception& err) {
    logger.log(err);
  } catch (const std::exception& err) {
    logger.error(err.what());
  }
}

void FtpServerConnection::stouFile(const std::string& cmd) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");

  try {
    auto conn = _pgDbPool.getConnection();
    pqxx::work w(*conn.getConnection());

    OidInBase oib{};
    if (writeFileToBase(cmd, w, oib)) {
      const auto fileUid = Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
      w.commit();
      closeDataConnection();
      sendReply(Poco::format("226 %s Transfer complete.", fileUid));
    }
  } catch (pqxx::pqxx_exception& err) {
    logger.error(err.base().what());
  } catch (Poco::Exception& err) {
    logger.log(err);
  } catch (std::exception& err) {
    logger.error(err.what());
  }
}

void FtpServerConnection::generateFilesList(Poco::Net::StreamSocket& socket, const std::string& cmd) {
  // _eStatus = FtpServerStatus::LISTING;

  sendReply("502 Command not implemented.");

  // for (auto& row : vec) {
  //   swf::archive::object::File tmpObj(row);
  //   auto fileExt = tmpObj.extension();
  //   if (!fileExt.empty()) {
  //     if (fileExt.find('.') == std::string::npos) {
  //       fileExt.insert(0, ".");
  //     }
  //   }
  //   auto listEntry = getFileListLine(tmpObj.size(), tmpObj.base.id(), fileExt, tmpObj.base.createTime(), cmd == "NLST", false);
  //   if (!listEntry.empty()) {
  //     sendData(std::move(listEntry), socket);
  //   }
  // }
}

void FtpServerConnection::listFiles(const std::string& cmd) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  try {
    if (!openDataConnection()) {
      return;
    }
    generateFilesList(_outSock, cmd);
    closeDataConnection();
    if (cmd != "STAT") {
      sendReply("226 Transfer complete.");
    } else {
      sendReply("213 End of status");
    }
  } catch (Poco::Exception& err) {
    sendReply("425 Can't open data connection.");
    logger.log(err);
    closeDataConnection();
  }
}

size_t FtpServerConnection::fileSize(uint32_t oid) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  try {
    if (oid == 0) {
      sendReply(Poco::format("550 File %d not found.", oid));
      return 0;
    }
    auto pgDbConnection = _pgDbPool.getConnection();

    pqxx::work w(*pgDbConnection.getConnection());
    pqxx::largeobjectaccess loa(w, oid, std::ios::in | std::ios::out);

    loa.seek(0, std::ios::end);
    const auto loaSize = loa.tell();
    loa.seek(static_cast<long>(_restartAt), std::ios::beg);
    return static_cast<size_t>(loaSize);
  } catch (Poco::Net::NetException& exc) {
    logger.log(exc);
  } catch (Poco::Exception& exc) {
    logger.log(exc);
  } catch (std::exception& err) {
    logger.error(err.what());
  }
  return 0;
}

void FtpServerConnection::executeProtocolFunction(Poco::StringTokenizer& receivedProtocolMessage) {
  if (receivedProtocolMessage.count() < 1) {
    sendReply("501 empty command.");
    return;
  }
  Poco::toUpperInPlace(receivedProtocolMessage[0]);
  const auto it = _listCmdFunction.find(Poco::hash(receivedProtocolMessage[0]));
  if (it != _listCmdFunction.end()) {
    it->second(receivedProtocolMessage);
  } else {
    sendReply("502 Command not implemented.");
  }
}

void FtpServerConnection::addFunction(const std::string& name, CallbackCmdFunction function) { _listCmdFunction.emplace(Poco::hash(name), std::move(function)); }

void FtpServerConnection::cmdNone(const Poco::StringTokenizer& data) { sendReply("500 Command not understood."); }

// Отключиться.
void FtpServerConnection::cmdQuit(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  sendReply("221 Goodbye.");
  _eStatus = FtpServerStatus::DISCONNECTED;
}

//Имя пользователя для входа на сервер.
void FtpServerConnection::cmdUser(const Poco::StringTokenizer& data) {
  if (_bIsLogged) {
    sendReply("332 abyrvalg");
    return;
  }
  if (data.count() < 2) {
    sendReply("501 Invalid number of arguments.");
  }
  if (_bIsLogged) {
    logOut();
  }
  _clientSessionId = data[1];
  logIn(_clientSessionId);
  if (!_bIsLogged) {
    sendReply("332	authentification required");
  }
}

//Войти в пассивный режим. Сервер вернёт адрес и порт, к которому нужно подключиться, чтобы забрать данные.
// Передача начнётся при введении следующих команд: RETR, LIST и т. д.
void FtpServerConnection::cmdPasv(const Poco::StringTokenizer& data) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (_eStatus != FtpServerStatus::WAITING && _eDataConnection == Dataconnection::PASV && _outSock.impl()->sockfd() != POCO_INVALID_SOCKET) {
    sendReply("425 You're already connected.");
    return;
  }
  // closeDataConnection();
  Poco::UInt16 randomDataPort = 0;
  while (true) {
    try {
      _ports.wait_dequeue(randomDataPort);
      const Poco::Net::SocketAddress sa(socket().address().host(), randomDataPort);
      _dataSock = std::make_unique<Poco::Net::ServerSocket>(sa);
      if (sa.port() == 0) {
        sendReply("451 Internal error - No more data ports available.");
        return;
      }
      Poco::StringTokenizer st(sa.host().toString(), ".", Poco::StringTokenizer::Options::TOK_TRIM | Poco::StringTokenizer::Options::TOK_IGNORE_EMPTY);
      std::stringstream ss;
      ss << "227 Entering Passive Mode (" << st[0] << "," << st[1] << "," << st[2] << "," << st[3] << "," << sa.port() / 256 << "," << sa.port() % 256 << ")";
      sendReply(ss.str());
      _eDataConnection = Dataconnection::PASV;
      break;
    } catch (Poco::Exception& exc) {
      logger.log(exc);
      logger.information("trying rebind socket");
    }
  }
}

void FtpServerConnection::cmdStru(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 1) {
    if (data[1] != "F") {
      sendReply("200 STRU Command Successful.");
    } else {
      sendReply("504 STRU failled. Parametre not implemented");
    }
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

void FtpServerConnection::cmdMode(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 1) {
    if (data[1] == "S") {  // Stream
      _eDataMode = DataTransferMode::STREAM;
      sendReply("200 MODE set to S.");
    } else if (data[1] == "Z") {
      sendReply("502 MODE Z non-implemented.");
    } else {
      sendReply("504 Unsupported transfer MODE.");
    }
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

//Установить тип передачи файла (бинарный, текстовый).
void FtpServerConnection::cmdType(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 1) {
    if (data[1] == "A") {
      _eDataType = DataTransferType::ASCII;
      sendReply("200 ASCII transfer mode active.");
    } else if (data[1] == "I") {
      _eDataType = DataTransferType::BINARY;
      sendReply("200 Binary transfer mode active.");
    } else {
      sendReply("550 Error - unknown binary mode.");
    }
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

void FtpServerConnection::cmdClnt(const Poco::StringTokenizer& data) { sendReply("200 Ok."); }

// Войти в активный режим. Например PORT 12,34,45,56,78,89.
// В отличие от пассивного режима для передачи данных сервер сам подключается к клиенту.
void FtpServerConnection::cmdPort(const Poco::StringTokenizer& data) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (_eStatus != FtpServerStatus::WAITING && _eDataConnection == Dataconnection::PORT && _outSock.impl()->sockfd() != POCO_INVALID_SOCKET) {
    sendReply("425 You're already connected.");
    return;
  }
  closeDataConnection();
  Poco::StringTokenizer st(data[1], ",");
  try {
    const Poco::Net::SocketAddress sa(Poco::format("%s.%s.%s.%s", st[0], st[1], st[2], st[3]),
                                      static_cast<Poco::UInt16>(std::stoi(std::string(st[4]))) * 256 + static_cast<Poco::UInt16>(std::stod(std::string(st[5]))));
    _sa = sa;
    _outSock.connect(_sa);
    if (_outSock.impl()->sockfd() != POCO_INVALID_SOCKET) {
      _eDataConnection = Dataconnection::PORT;
      sendReply("200 PORT command successful.");
    } else {
      sendReply("501 PORT address does not match originator.");
      closeDataConnection();
    }
  } catch (Poco::Exception& err) {
    sendReply("425 Failed to establish connection.");
    logger.log(err);
    closeDataConnection();
  }
}

// Пароль
void FtpServerConnection::cmdPass(const Poco::StringTokenizer& data) {
  //  //++pClient->nPasswordTries;
  //  // if (pClient->bIsLogged == true) {
  //  sendReply("230 User Logged In.");
  //  //}
  //  // else {
  //  //  if (pFtpServer->GetCheckPassDelay()) {
  //  //    sleep_msec(pFtpServer->GetCheckPassDelay());
  //  //  }
  //  //  pFtpServer->UserListLock.lock();
  //  //  if (pClient->pUser && pClient->pUser->bIsEnabled) {  // User is valid &
  //  //  enabled
  //  //    if ((pszCmdArg && !strcmp(pszCmdArg, pClient->pUser->szPassword)) ||
  //  //    !*pClient->pUser->szPassword) { // Password is valid.
  //  //      if (pClient->pUser->uiMaxNumberOfClient == 0 ||
  //  //      pClient->pUser->uiNumberOfClient <
  //  //      pClient->pUser->uiMaxNumberOfClient) {
  //  //        pClient->LogIn();
  //  //      }
  //  //      else {
  //  //        SendReply("421 Too many users logged in for this account.");
  //  //      }
  //  //    }
  //  //    else if (!pszCmdArg) {
  //  //      SendReply("501 Invalid number of arguments.");
  //  //    }
  //  //  }
  //  //  pFtpServer->UserListLock.unlock();
  //  //  if (pClient->bIsLogged) {
  //  //    return;
  //  //  }
  //  //}
  //  // if (pFtpServer->GetMaxPasswordTries() && pClient->nPasswordTries >=
  //  // pFtpServer->GetMaxPasswordTries()) {
  //  //  break;
  //  //}
  //  // SendReply("530 Login or password incorrect!");
  sendReply("502 Command not implemented.");
}

// Возвращает список файлов директории. Список передаётся через соединение данных.
void FtpServerConnection::cmdList(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data[0] == "STAT" && data.count() == 1) {
    sendReply("211 :: DocStorageFTP / Browser FTP Server:: thebrowser@gmail.com");
    return;
  }
  if (_eStatus != FtpServerStatus::WAITING) {
    sendReply("425 You're already connected.");
    return;
  }
  if (data.has("-a")) {
    _currentTransfer.optA = true;
  }
  if (data.has("-d")) {
    _currentTransfer.optD = true;
  }
  if (data.has("-F")) {
    _currentTransfer.optF = true;
  }
  if (data.has("-l")) {
    _currentTransfer.optL = true;
  }

  if (data[0] == "STAT") {
    sendReply("213-Status follows:");
  }

  listFiles(data[0]);
}

// Возвращает список файлов директории в более кратком формате, чем LIST. Список передаётся через соединение данных.
void FtpServerConnection::cmdNlst(const Poco::StringTokenizer& data) { cmdList(data); }

//  Сменить директорию.
void FtpServerConnection::cmdXcwd(const Poco::StringTokenizer& data) { cmdCwd(data); }
void FtpServerConnection::cmdCwd(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() >= 2) {
    _szWorkingDir = data[1];
    sendReply("250 CWD command successful.");
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

void FtpServerConnection::cmdFeat(const Poco::StringTokenizer& data) {
  sendReply(
      "211-Features:\r\n"
      " CLNT\r\n"
      " MDTM\r\n"
      " REST STREAM\r\n"
      " SIZE\r\n"
      "211 End");
}

//  Возвращает время модификации файла.
void FtpServerConnection::cmdMdtm(const Poco::StringTokenizer& data) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 0) {
    if (data[1] == "/") {
      sendReply("550 No such file or directory.");
      return;
    }
    Poco::DateTime pdt;
    std::stringstream ss;
    ss << "213 " << pdt.year() << pdt.month() << pdt.day() << pdt.hour() << pdt.minute() << pdt.second();
    sendReply(ss.str());
    logger.information(ss.str());
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

//Возвращает текущую директорию.
void FtpServerConnection::cmdXpwd(const Poco::StringTokenizer& data) { cmdPwd(data); }
void FtpServerConnection::cmdPwd(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  std::string tmpString("257 ");
  if (_szWorkingDir != "/") {
    tmpString.append("/").append(_szWorkingDir);
  } else {
    tmpString.append("\"/\"");
  }
  tmpString.append(" is current directory.");
  sendReply(tmpString);
}

// Сменить директорию на вышестоящую.
void FtpServerConnection::cmdCdup(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  _szWorkingDir = "/";
  sendReply("250 CDUP command successful.");
}

void FtpServerConnection::cmdXcup(const Poco::StringTokenizer& data) { cmdCdup(data); }

void FtpServerConnection::cmdStat(const Poco::StringTokenizer& data) { sendReply("550 "); }

//Прервать передачу файла
void FtpServerConnection::cmdAbor(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (_eStatus != FtpServerStatus::WAITING) {
    closeDataConnection();
    sendReply("426 Previous command has been finished abnormally.");
  }
  sendReply("226 ABOR command successful.");
}

void FtpServerConnection::cmdRest(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 1 && _eStatus == FtpServerStatus::WAITING) {
    _restartAt = std::stol(data[1]);
    sendReply("350 REST command successful.");
  } else {
    sendReply("501 Syntax error in arguments.");
  }
}

//Скачать файл. Перед RETR должна быть команда PASV или PORT.
void FtpServerConnection::cmdRetr(const Poco::StringTokenizer& data) {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() < 2) {
    sendReply("501 Syntax error in arguments.");
    return;
  }

  try {
    Poco::Path p(data[1]);
    retrieveData(p.getBaseName());
  } catch (std::exception& err) {
    logger.error(err.what());
  }
}

//Закачать файл.Перед STOU должна быть команда PASV или PORT.
void FtpServerConnection::cmdStou(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  stouFile(data[0]);
}

//Закачать файл.Перед STOR должна быть команда PASV или PORT.
void FtpServerConnection::cmdStor(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() < 2) {
    sendReply("501 Syntax error in arguments.");
    return;
  }
  Poco::Path p(data[1]);
  std::string fname{p.getBaseName()};
  if (fname.empty()) {
    sendReply(Poco::format("501 Syntax error in arguments. %s", data[1]));
    return;
  }
  auto& logger = Poco::Logger::get("docstorage");
  logger.trace(Poco::format("%d on cmd stor", static_cast<int>(socket().impl()->sockfd())));
  storFile(data[0], fname);
}

//Возвращает размер файла.
void FtpServerConnection::cmdSize(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() < 2) {
    sendReply("501 Syntax error in arguments.");
  }
  Poco::Path p(data[1]);
  std::string fname{p.getBaseName()};
  if (fname.empty()) {
    sendReply(Poco::format("550 File %s not found", data[1]));
    return;
  }
  auto pgConnection = _pgDbPool.getConnection();
  pqxx::work w(*pgConnection.getConnection());
  auto result = w.exec(Poco::format("SELECT size FROM entry WHERE guid IN (SELECT internal_uid FROM storage WHERE external_uid = \'%s\')", fname));
  if (result.empty()) {
    sendReply(Poco::format("550 File %s not found", data[1]));
    return;
  }
  sendReply(Poco::format("213 %z", result.begin()->begin().as<size_t>()));
}

// Удалить файл (DELE filename).
void FtpServerConnection::cmdDele(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() < 2) {
    sendReply("501 Syntax error in arguments.");
  }
  // Проверка наличия файла
  Poco::Path pp(data[1]);
  auto fName = pp.getBaseName();
  if (fName.empty()) {
    sendReply(Poco::format("501 Error argument name: %s", data[1]));
    return;
  }
  deleteFile(fName);
}

void FtpServerConnection::cmdOpts(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() > 0) {
    if (data[1] == "utf8") {
      sendReply("202 UTF8 mode is always enabled. No need to send this command.");
      return;
    }
    sendReply("501 Option not understood.");
  } else {
    sendReply("501 Invalid number of arguments.");
  }
}

void FtpServerConnection::cmdMd5(const Poco::StringTokenizer& data) {
  if (!_bIsLogged) {
    sendReply("332	authentification required");
    return;
  }
  if (data.count() < 3) {
    sendReply("501 Invalid number of arguments.");
    return;
  }
  auto cmd = data[0];
  const auto fileName = data[1];
  const auto md5 = data[2];

  auto vec(getFileEntry(fileName));
  if (vec.empty()) {
    sendReply(Poco::format("550 File %s not found.", fileName));
    closeDataConnection();
    return;
  }

  try {
    // auto fvec =
    //     swf::repository::queries::SimpleFind::exec<std::string>(swf::FIELD_AND(swf::FIELD_EQUAL_TYPE(swf::archive::object::file::option::file), swf::FIELD_EQUAL_STRING(swf::FIELD_BASE("md5"),
    //     md5)));
    // if (fvec.empty()) {
    //   sendReply(Poco::format("550	MD5 %s not found", md5));
    //   return;
    // }
    // swf::repository::object::change::List cList;
    // swf::archive::object::File tmpObj(vec[0]);
    // cList.add(std::string("object.").append(tmpObj.base.type()).append(".body_id"), std::string());
    // cList.add("object." + tmpObj.base.type() + ".size", std::string());
    // cList.add("object." + tmpObj.base.type() + ".md5", md5);
    // swf::repository::queries::ChangeObjectById::exec(tmpObj.base.id(), cList);
  } catch (std::runtime_error& ex) {
    // TODO internal error
    sendReply(std::string("451 Unable update object. ").append(ex.what()));
    return;
  }
  sendReply(Poco::format("200 md5 %s exist", md5));
}

void FtpServerConnection::logIn(const std::string& client) {
  _bIsLogged = true;
  sendReply(Poco::format("230 User %s Logged In.", client));
}

void FtpServerConnection::logOut() {
  if (_bIsLogged) {
    _bIsLogged = false;
    _szWorkingDir = "/";
    _clientSessionId.clear();
  }
}

std::string FtpServerConnection::getFileListLine(
    const int64_t size, const std::string& pszName, const std::string& pszNameExt, const std::string& dateCreate, const bool simple, const bool prFolder) const {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  if (pszName.empty() || size < 1) {
    return {};
  }

  if (simple) {
    return _szWorkingDir + pszName + pszNameExt;
  }

  try {
    const Poco::LocalDateTime pldt;
    return Poco::format(
        "%c%c%c%cr--r-- 1 user group %Ld %s %s%s %s\r\n", (!prFolder) ? '-' : 'd', 'r', 'w', '-', size, Poco::DateTimeFormatter::format(pldt, "%b %d %H:%M"), pszName, pszNameExt, std::string());
  } catch (Poco::Exception& err) {
    logger.error(err.displayText());
  } catch (std::exception& err) {
    logger.error(err.what());
  }
  return {};
}

FtpServerConnection::FtpServerConnection(const Poco::Net::StreamSocket& socket,
                                         PgDbPool& pgDbPool,
                                         /*swf::repository::engine::ContextFactory& contextFactory,*/
                                         moodycamel::BlockingConcurrentQueue<uint16_t>& ports)
    : TCPServerConnection(socket), _pgDbPool(pgDbPool), _ports(ports), _timeOut(20), _buff() {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  logger.information(Poco::format("new client: %d", static_cast<int>(socket.impl()->sockfd())));
}

FtpServerConnection::FtpServerConnection(const Poco::Net::StreamSocket& socket, PgDbPool& pgDbPool, moodycamel::BlockingConcurrentQueue<uint16_t>& ports, Poco::Timespan& timeOut)
    : TCPServerConnection(socket), _pgDbPool(pgDbPool), _ports(ports), _timeOut(timeOut), _buff() {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  logger.information(Poco::format("new client: %d", static_cast<int>(socket.impl()->sockfd())));
}

FtpServerConnection::~FtpServerConnection() {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  logger.information("client %d disconnected", static_cast<int>(socket().impl()->sockfd()));
}

void FtpServerConnection::run() {
  Poco::Logger& logger = Poco::Logger::get("docstorage");
  std::string bytesBuffer(65535, 0);
  auto sockImpl = socket().impl();
  try {
    initProtocolFunction();
    sendReply("220 Browser Ftp Server.");
    int receivedBytes = 0;
    do {
      try {
        socket().setReceiveTimeout(_timeOut);
        receivedBytes = socket().receiveBytes(&bytesBuffer[0], static_cast<int>(bytesBuffer.size()), MSG_NOSIGNAL);
        if (receivedBytes == 0) {
          logger.information("client %d closed connection", static_cast<int>(sockImpl->sockfd()));
          break;
        }
        if (receivedBytes < 0) {
          logger.error("error on receive commands from client %d, error: %d", static_cast<int>(sockImpl->sockfd()), sockImpl->socketError());
        }
      } catch (Poco::TimeoutException& exc) {
        if ((_eStatus != FtpServerStatus::DISCONNECTED && _eStatus != FtpServerStatus::WAITING) || exc.code() == POCO_EAGAIN) {
          continue;
        }

        logger.warning("timeout on receive commands from client %d, error: %d : %d", static_cast<int>(sockImpl->sockfd()), sockImpl->socketError(), static_cast<int>(Poco::Error::last()));
        break;
      } catch (Poco::Net::NetException& err) {
        logger.log(err);
        break;
      }
      bytesBuffer.at(static_cast<size_t>(receivedBytes)) = '\0';
      Poco::StringTokenizer lines(bytesBuffer, "\r\n", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
      for (auto& line : lines) {
        if (!line.empty() && line[0] != 0) {
          logger.information("%d Q: %s", static_cast<int>(sockImpl->sockfd()), line);
          Poco::StringTokenizer command(line, " ", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
          executeProtocolFunction(command);
        }
      }
      if (_eStatus == FtpServerStatus::DISCONNECTED) {
        break;
      }
    } while (receivedBytes > 0);
  } catch (Poco::Exception& err) {
    logger.log(Poco::format("client %d: %s", sockImpl->sockfd(), err.displayText()));
    // _isTransactError = true;
  } catch (std::exception& err) {
    logger.error(Poco::format("client %d: %s", sockImpl->sockfd(), err.what()));
    // _isTransactError = true;
  }
}

// Empty operation
void FtpServerConnection::cmdNoop(const Poco::StringTokenizer& data) { sendReply("200 NOOP Command Successful."); }

void FtpServerConnection::cmdAllo(const Poco::StringTokenizer& data) { sendReply("200 ALLO Command Successful."); }

void FtpServerConnection::cmdSite(const Poco::StringTokenizer& data) { sendReply("502 Not implemented yet."); }

// prints ftp server commands list
void FtpServerConnection::cmdHelp(const Poco::StringTokenizer& data) { sendReply("200 No Help Available."); }

// returns system type (UNIX, WIN, …).
void FtpServerConnection::cmdSyst(const Poco::StringTokenizer& data) { sendReply("215 UNIX Type: L8"); }

void FtpServerConnection::cmdAppe(const Poco::StringTokenizer& data) { cmdStor(data); }

// Rename file RNFR — src, RNTO — dst.
void FtpServerConnection::cmdRnfr(const Poco::StringTokenizer& data) { sendReply("502 Not implemented yet."); }
// Rename file RNFR — src, RNTO — dst.
void FtpServerConnection::cmdRnto(const Poco::StringTokenizer& data) { sendReply("502 Not implemented yet."); }
// Create directory
void FtpServerConnection::cmdMkd(const Poco::StringTokenizer& data) { sendReply("502 Command MKD not implemented."); }
void FtpServerConnection::cmdXmkd(const Poco::StringTokenizer& data) { cmdMkd(data); }

// Remove directory
void FtpServerConnection::cmdRmd(const Poco::StringTokenizer& data) { sendReply("502 Not implemented yet."); }
void FtpServerConnection::cmdXrmd(const Poco::StringTokenizer& data) { cmdRmd(data); }

void FtpServerConnection::initProtocolFunction() {
  addFunction("ABOR", std::bind(&FtpServerConnection::cmdAbor, this, std::placeholders::_1));
  addFunction("ALLO", std::bind(&FtpServerConnection::cmdAllo, this, std::placeholders::_1));
  addFunction("APPE", std::bind(&FtpServerConnection::cmdStor, this, std::placeholders::_1));

  addFunction("CDUP", std::bind(&FtpServerConnection::cmdCdup, this, std::placeholders::_1));
  addFunction("CLNT", std::bind(&FtpServerConnection::cmdClnt, this, std::placeholders::_1));
  addFunction("CWD", std::bind(&FtpServerConnection::cmdCwd, this, std::placeholders::_1));

  addFunction("DELE", std::bind(&FtpServerConnection::cmdDele, this, std::placeholders::_1));

  addFunction("FEAT", std::bind(&FtpServerConnection::cmdFeat, this, std::placeholders::_1));

  addFunction("LIST", std::bind(&FtpServerConnection::cmdList, this, std::placeholders::_1));

  addFunction("HELP", std::bind(&FtpServerConnection::cmdHelp, this, std::placeholders::_1));

  addFunction("MDTM", std::bind(&FtpServerConnection::cmdMdtm, this, std::placeholders::_1));
  addFunction("MKD", std::bind(&FtpServerConnection::cmdMkd, this, std::placeholders::_1));
  addFunction("MODE", std::bind(&FtpServerConnection::cmdMode, this, std::placeholders::_1));

  addFunction("NLST", std::bind(&FtpServerConnection::cmdNlst, this, std::placeholders::_1));
  addFunction("NONE", std::bind(&FtpServerConnection::cmdNone, this, std::placeholders::_1));
  addFunction("NOOP", std::bind(&FtpServerConnection::cmdNoop, this, std::placeholders::_1));

  addFunction("OPTS", std::bind(&FtpServerConnection::cmdOpts, this, std::placeholders::_1));

  addFunction("PASS", std::bind(&FtpServerConnection::cmdPass, this, std::placeholders::_1));
  addFunction("PASV", std::bind(&FtpServerConnection::cmdPasv, this, std::placeholders::_1));
  addFunction("PORT", std::bind(&FtpServerConnection::cmdPort, this, std::placeholders::_1));
  addFunction("PWD", std::bind(&FtpServerConnection::cmdPwd, this, std::placeholders::_1));

  addFunction("QUIT", std::bind(&FtpServerConnection::cmdQuit, this, std::placeholders::_1));

  addFunction("REST", std::bind(&FtpServerConnection::cmdRest, this, std::placeholders::_1));
  addFunction("RETR", std::bind(&FtpServerConnection::cmdRetr, this, std::placeholders::_1));
  addFunction("RMD", std::bind(&FtpServerConnection::cmdRmd, this, std::placeholders::_1));
  addFunction("RNFR", std::bind(&FtpServerConnection::cmdRnfr, this, std::placeholders::_1));
  addFunction("RNTO", std::bind(&FtpServerConnection::cmdRnto, this, std::placeholders::_1));

  addFunction("SITE", std::bind(&FtpServerConnection::cmdSite, this, std::placeholders::_1));
  addFunction("SIZE", std::bind(&FtpServerConnection::cmdSize, this, std::placeholders::_1));
  addFunction("STAT", std::bind(&FtpServerConnection::cmdList, this, std::placeholders::_1));
  addFunction("STOR", std::bind(&FtpServerConnection::cmdStor, this, std::placeholders::_1));
  addFunction("STOU", std::bind(&FtpServerConnection::cmdStor, this, std::placeholders::_1));
  addFunction("STRU", std::bind(&FtpServerConnection::cmdStru, this, std::placeholders::_1));
  addFunction("SYST", std::bind(&FtpServerConnection::cmdSyst, this, std::placeholders::_1));

  addFunction("TYPE", std::bind(&FtpServerConnection::cmdType, this, std::placeholders::_1));

  addFunction("USER", std::bind(&FtpServerConnection::cmdUser, this, std::placeholders::_1));

  addFunction("XCUP", std::bind(&FtpServerConnection::cmdXcup, this, std::placeholders::_1));
  addFunction("XCWD", std::bind(&FtpServerConnection::cmdXcwd, this, std::placeholders::_1));
  addFunction("XMKD", std::bind(&FtpServerConnection::cmdXmkd, this, std::placeholders::_1));
  addFunction("XPWD", std::bind(&FtpServerConnection::cmdXpwd, this, std::placeholders::_1));
  addFunction("XRMD", std::bind(&FtpServerConnection::cmdXrmd, this, std::placeholders::_1));
  addFunction("MD5", std::bind(&FtpServerConnection::cmdMd5, this, std::placeholders::_1));
}
