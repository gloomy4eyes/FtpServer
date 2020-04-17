#pragma once

#include <functional>
#include <string>
#include <map>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/ServerSocket.h>
#include <random>
//#include "BlockingConcurrentQueue.hpp"
//#include <pqxx/transaction.hxx>
#include <unordered_map>

class PgDbPool;

/// Enum the differents Modes of Transfer.
enum class DataTransferMode { STREAM, ZLIB };

namespace Poco {
class StringTokenizer;
namespace Net {
class Socket;
class StreamSocket;
}  // namespace Net
}  // namespace Poco

/// Enum the differents Type of Transfer.
enum class DataTransferType {
  ASCII,
  BINARY,  ///< equals to IMAGE
  EBCDIC
};

/// Enum the differents Modes of Connection for transfering Data.
enum class Dataconnection { NONE, PASV, PORT };

enum class FtpServerStatus { WAITING, LISTING, UPLOADING, DOWNLOADING, DISCONNECTED };

enum class FtpOperation { READFILE = 0x1, WRITEFILE = 0x2, DELETEFILE = 0x4, LIST = 0x8, CREATEDIR = 0x10, DELETEDIR = 0x20 };

struct DatatransferT {
  bool optA{false};
  bool optD{false};
  bool optF{false};
  bool optL{false};
};

class PgDbConnection;

using CallbackCmdFunction = std::function<void(const Poco::StringTokenizer&)>;

constexpr int frameSize = 1048576 * 5;  // 5MB;

class FtpServerConnection : public Poco::Net::TCPServerConnection {
 public:
//  FtpServerConnection(const Poco::Net::StreamSocket& socket, PgDbPool& pgDbPool, moodycamel::BlockingConcurrentQueue<uint16_t>& ports);
//  FtpServerConnection(const Poco::Net::StreamSocket& socket, PgDbPool& pgDbPool, moodycamel::BlockingConcurrentQueue<uint16_t>& ports, Poco::Timespan& timeOut);
  ~FtpServerConnection() override;
  void run() override;

 private:
  PgDbPool& _pgDbPool;
  std::string getFileListLine(int64_t size, const std::string& pszName, const std::string& pszNameExt, const std::string& dateCreate, bool simple, bool prFolder = false) const;
  DataTransferMode _eDataMode{DataTransferMode::STREAM};
  DataTransferType _eDataType{DataTransferType::ASCII};
  FtpServerStatus _eStatus{FtpServerStatus::WAITING};
  Dataconnection _eDataConnection{Dataconnection::NONE};
  std::string _szWorkingDir{"/"};
  DatatransferT _currentTransfer{};
  bool _bIsLogged{false};
  std::unique_ptr<Poco::Net::ServerSocket> _dataSock;
  Poco::Net::StreamSocket _outSock{};
  long _restartAt{0};
  std::string _clientSessionId;
  Poco::Net::SocketAddress _sa;
  std::unordered_map<size_t, CallbackCmdFunction> _listCmdFunction;
//  moodycamel::BlockingConcurrentQueue<uint16_t>& _ports;
  Poco::Timespan _timeOut;
  std::random_device _rd;
  std::string _uid;
  std::array<char, frameSize> _buff;
  struct OidInBase {
    uint64_t oid;
    uint64_t size;
  };

  bool openDataConnection();   ///< Open the Data Channel in order to transmit data.
  void closeDataConnection();  ///< Close the Data Channel.
  static void sendData(std::string bytesBuffer, Poco::Net::StreamSocket& socket);
  void sendReply(std::string message);

  // Потоки по обработке запросов
  virtual void retrieveData(const std::string& dataId);
  std::vector<std::string> getFileEntry(const std::string& fileName);
  // virtual void deleteFile(const std::string& fileName, swf::archive::object::File dbFileJsonEntry);
  virtual void deleteFile(const std::string& fileName);
  // void updateEntry(std::unique_ptr<swf::archive::FileManager>& fileManager, uint64_t dataLength, uint64_t bodyId);
  virtual void storFile(const std::string& cmd, const std::string& fileName);
  virtual void stouFile(const std::string& cmd);
  virtual void generateFilesList(Poco::Net::StreamSocket& socket, const std::string& cmd);
  //  virtual std::vector<std::string> getAllFilesEntries();
  virtual void listFiles(const std::string& cmd);
  size_t fileSize(uint32_t oid);

//  bool writeFileToBase(const std::string& cmd, pqxx::work& w, OidInBase& oib);

  void initProtocolFunction();
  void logIn(const std::string& client);
  void logOut();
  void executeProtocolFunction(Poco::StringTokenizer& receivedProtocolMessage);
  void addFunction(const std::string& name, CallbackCmdFunction function);

  // Функции обработки протокола
  void cmdNone(const Poco::StringTokenizer& data);
  void cmdQuit(const Poco::StringTokenizer& data);
  void cmdUser(const Poco::StringTokenizer& data);
  void cmdPass(const Poco::StringTokenizer& data);
  void cmdNoop(const Poco::StringTokenizer& data);
  void cmdAllo(const Poco::StringTokenizer& data);
  void cmdSite(const Poco::StringTokenizer& data);
  void cmdHelp(const Poco::StringTokenizer& data);
  void cmdSyst(const Poco::StringTokenizer& data);
  void cmdStru(const Poco::StringTokenizer& data);
  void cmdMode(const Poco::StringTokenizer& data);
  void cmdType(const Poco::StringTokenizer& data);
  void cmdClnt(const Poco::StringTokenizer& data);
  void cmdPort(const Poco::StringTokenizer& data);
  void cmdPasv(const Poco::StringTokenizer& data);
  void cmdList(const Poco::StringTokenizer& data);
  void cmdNlst(const Poco::StringTokenizer& data);
  void cmdCwd(const Poco::StringTokenizer& data);
  void cmdXcwd(const Poco::StringTokenizer& data);
  void cmdFeat(const Poco::StringTokenizer& data);
  void cmdMdtm(const Poco::StringTokenizer& data);
  void cmdPwd(const Poco::StringTokenizer& data);
  void cmdXpwd(const Poco::StringTokenizer& data);
  void cmdCdup(const Poco::StringTokenizer& data);
  void cmdXcup(const Poco::StringTokenizer& data);
  void cmdStat(const Poco::StringTokenizer& data);
  void cmdAbor(const Poco::StringTokenizer& data);
  void cmdRest(const Poco::StringTokenizer& data);
  void cmdRetr(const Poco::StringTokenizer& data);
  void cmdStor(const Poco::StringTokenizer& data);
  void cmdAppe(const Poco::StringTokenizer& data);
  void cmdStou(const Poco::StringTokenizer& data);
  void cmdSize(const Poco::StringTokenizer& data);
  void cmdDele(const Poco::StringTokenizer& data);
  void cmdRnfr(const Poco::StringTokenizer& data);
  void cmdRnto(const Poco::StringTokenizer& data);
  void cmdMkd(const Poco::StringTokenizer& data);
  void cmdXmkd(const Poco::StringTokenizer& data);
  void cmdRmd(const Poco::StringTokenizer& data);
  void cmdXrmd(const Poco::StringTokenizer& data);
  void cmdOpts(const Poco::StringTokenizer& data);
  void cmdMd5(const Poco::StringTokenizer& data);
};

// USER - Имя пользователя
// TYPE - Задать режим представления данных(Бинарный, текстовый)
// SYST - Показать идентификацию операционной системы
// STRU - Указать характеристику файловой структуры
// STOU - Сохранить файл с присвоением уникального имени
// STOR - Сохранить файл
// STAT - Показать состояние
// SMNT - Подключить структуру данных файловой системы
// SITE - Управление параметрами сайта (сервера)
// RNTO - Переименовать в (имя)
// RNFR - Переименовать из (имя)
// RMD - Удалить каталог
// RETR - Получить файл
// REST - Возобновить передачу файла
// REIN - Начать сессию заново
// QUIT - Завершить сеанс
// PWD - Показать рабочий каталог
// PORT - Установить активный режим (Например PORT 12,34,45,56,78,89. В отличие от пассивного режима для передачи данных сервер сам подключается к клиенту. )
// PASV - Установить пассивный режим (Сервер вернет адрес и порт к которому нужно подключиться чтобы забрать данные)
// PASS - Пароль пользователя NOP - Нет операции
// NLST - Получить список имён содержимого каталога
// MODE - Задать режим передачи данных
// MKD - Создать каталог
// LIST - Получить список содержимого каталога
// HELP - Показать справку (Выводит список команд принимаемых сервером)
// DELE - Удалить файл (DELE имя_файла)
// CWD - Сменить рабочий каталог
// CDUP - Перейти в родительский каталог
// APPE - Добавить в конец файла (с созданием)
// ALLO - Зарезервировать место на диске
// ACCT - Аккаунт пользователя
// ABOR - Прервать выполнение команды

//Всякие разные команды в том числе и полезные:
// MDTM - Получить время модификации файла RFC 3659
// AUTH - Аутентификация/ Механизм безопасности RFC 2773, RFC 4217
// MLST - Получить характеристики одного объекта RFC 3659
// MLSD - Получить список объектов каталога с характеристиками (для машины) RFC 3659
// PBSZ - Установить размер буфера защиты RFC 4217 PROT - Установить уровень защиты канала данных - RFC 4217
// REST - Возобновить передачу файла (для режима STREAM) - RFC 3659[3]
// SIZE - Показать размер файла - RFC 3659
// LANG - Задать язык (для сообщений сервера) RFC 2640 Расширения системы безопасности FTP (RFC2228):
// PROT - Установить уровень защиты канала данных
// PBSZ - Установить размер буфера защиты
// MIC - Команда, защищенная по уровню «Целостность»
// ENC - Команда, защищенная по уровню «Приватность»
// CONF - Команда, защищенная по уровню «Конфиденциальность»
// CCC - Очистить канал команд
// AUTH - Аутентификация/ Механизм безопасности
// ADAT - Аутентификация/ Данные о безопасности

//Команды для NAT/IPv6 RFC (2428):
// EPSV - Установить пассивный режим для IPv6 (расширенный запрос порта данных)
// EPRT - Установить активный режим для IPv6 (расширенное задание порта данных)

//Механизм регистрации и распознавания расширений FTP (RFC2389):
// OPTS - Установить опции функции
// FEAT - Согласование функций

//Команды которые ныне не используются или экспериментальные, в общем не стоит к ним обращаться(RFC 775, RFC 1123 RFC 1545, RFC 1639):
// XRMD - предшественник RMD
// XPWD - предшественник PWD
// XMKD - предшественник MKD RFC
// XCWD - предшественник CWD
// XCUP - предшественник CDUP
// LPSV - Установить пассивный режим (запрос порта данных, FOOBAR)
// LPRT - Установить активный режим (задание порта данных, FOOBAR)
