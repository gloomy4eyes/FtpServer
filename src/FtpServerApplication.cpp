#include "ftpserver/FtpServerApplication.h"

#include "ftpserver/FtpServerConnectionFactory.h"
#include "ftpserver/PgDbPool.h"

#include <iostream>

#include <Poco/Net/TCPServer.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/AsyncChannel.h>
#include <Poco/File.h>


namespace ftp {
namespace server {

FtpServerApplication::FtpServerApplication() {}

void FtpServerApplication::defineOptions(Poco::Util::OptionSet &options) {
  ServerApplication::defineOptions(options);

  options.addOption(Poco::Util::Option("help", "h", "display help information on command line arguments").required(false).repeatable(false));

  options.addOption(
      Poco::Util::Option("version", "v", "display version information").required(false).repeatable(false).callback(Poco::Util::OptionCallback<FtpServerApplication>(this, &FtpServerApplication::handleVersion)));

  options.addOption(Poco::Util::Option("fileconfig", "f", "path to config file")
                        .required(false)
                        .repeatable(false)
                        .argument("file")
                        .callback(Poco::Util::OptionCallback<FtpServerApplication>(this, &FtpServerApplication::handleFile)));

  options.addOption(Poco::Util::Option("log_level", "l", "log level [1..8]")
                        .required(false)
                        .repeatable(false)
                        .argument("number")
                        .callback(Poco::Util::OptionCallback<FtpServerApplication>(this, &FtpServerApplication::handleLog)));
}

void FtpServerApplication::handleOption(const std::string &name, const std::string &value) {
  ServerApplication::handleOption(name, value);

  if (name == "help") {
    _helpRequested = true;
  } else if (name == "log_level") {
    handleLog(name, value);
  } else if (name == "version") {
    handleVersion(name, value);
  } else if (name == "fileconfig") {
    handleFile(name, value);
  }
}

void FtpServerApplication::displayHelp() {
  Poco::Util::HelpFormatter helpFormatter(options());
  helpFormatter.setCommand(commandName());
  helpFormatter.setUsage("OPTIONS");
  helpFormatter.setHeader("Classifier server.");
  helpFormatter.format(std::cout);
  stopOptionsProcessing();
}

void FtpServerApplication::initLogger(std::string const &logFilePath, const Poco::Message::Priority prio, std::string const &loggerName) {
  Poco::File f(Poco::Path(logFilePath).parent());
  if (!f.exists()) {
    f.createDirectories();
  }
  Poco::AutoPtr<Poco::FileChannel> pFileChannel(new Poco::FileChannel(logFilePath));
  pFileChannel->setProperty("rotation", "8 M");
  pFileChannel->setProperty("archive", "timestamp");
  pFileChannel->setProperty("times", "local");
  pFileChannel->setProperty("compress", "true");
  pFileChannel->setProperty("purgeAge", "1 months");
  // pFileChannel->setProperty("", "");

  Poco::AutoPtr<Poco::PatternFormatter> pPatternFormatter(new Poco::PatternFormatter);
  pPatternFormatter->setProperty("pattern", "[%Y-%m-%d %H:%M:%S:%i] (%p) %t");
  Poco::AutoPtr<Poco::FormattingChannel> pFormattingChannel(new Poco::FormattingChannel(pPatternFormatter, pFileChannel));

  Poco::AutoPtr<Poco::AsyncChannel> pAsync(new Poco::AsyncChannel(pFormattingChannel.get()));
  Poco::Logger::create(loggerName, pAsync, prio);
  logger().setChannel(pAsync);
}

int FtpServerApplication::main(const std::vector<std::string> &args) {
  if (_helpRequested) {
    displayHelp();
    return Application::EXIT_OK;
  }
  if (_versionRequested) {
    return Application::EXIT_OK;
  }
  if (_configFilePath.toString().empty()) {
    return Application::EXIT_CONFIG;
  }

//  initLogger("log/ftpserver.log", static_cast<Poco::Message::Priority>(config().log.level), "ftpserver");

//  if (swfConfig().empty()) {
//    swfConfig().service.name = name();
//    swfConfig().service.threads = 1;
//    swfConfig().net.port = DEFAULT_PORT;
//    swfConfig().net.defaultServerID = "O1$$";
//    swfConfig().broker.uri = "tcp://127.0.0.1:12345?transport.trace=false";
//    swfConfig().broker.queue = "DOCSTORAGE";
//    swfConfig().broker.transportDB = std::string(name()).append(".transport.db");
//    swfConfig().log.level = 8;
//    swfConfig().database.connection = "dbname=DOKABASE user=postgres password=postgres hostaddr=127.0.0.1 port=5432";
//  }
//
//  if (swfConfig().database.connection.empty()) {
//    return Application::EXIT_CONFIG;
//  }

  const Poco::UInt16 startPort = config().getUInt("docstorage.data_start_port", 22323);
  const Poco::UInt16 portRange = config().getUInt("docstorage.data_ports_range", 20000);
  Poco::Timespan timeOut(config().getUInt("docstorage.timeout_seconds", 20), 0);
  const auto maxPoolCount = config().getUInt("docstorage.pool_count_max", 32);
  const auto maxServerQueue = config().getUInt("docstorage.queue_max", 512);

  Poco::Logger &loggerr = logger().get("docstorage");
  try {
//    moodycamel::BlockingConcurrentQueue<uint16_t> ports;
//    for (uint16_t i = startPort; i < (startPort + portRange); ++i) {
//      ports.enqueue(i);
//    }

    PgDbPool pgDbPool(config().getString("docstorage.dbms_data_connection_string", ""), static_cast<uint8_t>(config().getUInt("docstorage.dbms_data_connection_pcnt", 10)));
    Poco::ThreadPool tp(2, maxPoolCount);
    Poco::AutoPtr<Poco::Net::TCPServerParams> serverParams(new Poco::Net::TCPServerParams);
    serverParams->setMaxQueued(maxServerQueue);
//    Poco::Net::TCPServer server(new FtpServerConnectionFactory(pgDbPool, swfConfig().database.connection, ports, config().getUInt("docstorage.threads", 10), timeOut),
//                                tp,
//                                Poco::Net::ServerSocket(static_cast<Poco::UInt16>(swfConfig().net.port)),
//                                serverParams);
//    server.start();
//    std::cout << "FTP server starts on port: " << swfConfig().net.port << std::endl;
//    std::cout << "data base connection: " << swfConfig().database.connection << std::endl;

    waitForTerminationRequest();

//    server.stop();
  } catch (Poco::Exception &exc) {
    loggerr.log(exc);
  } catch (std::exception &err) {
    loggerr.fatal(err.what());
  }

  return Application::EXIT_OK;
}

void FtpServerApplication::handleVersion(const std::string &name, const std::string &value) {
  if (name == "version") {
    _versionRequested = true;
    stopOptionsProcessing();
  }
}

void FtpServerApplication::handleFile(const std::string &name, const std::string &value) { _configFilePath.assign(value); }

//void FtpServerApplication::handleLog(const std::string &name, const std::string &value) { swfConfig().log.level = std::stoi(value); }

void FtpServerApplication::stop() { _isRunning = false; }

bool FtpServerApplication::isRunning() const { return _isRunning; }

bool FtpServerApplication::started() const { return _started; }
}  // namespace server
}  // namespace ftp
