#pragma once

#include <Poco/Util/ServerApplication.h>

namespace ftp {
   namespace server {
      class FtpServerApplication : public Poco::Util::ServerApplication {
      public:
         FtpServerApplication();
         void stop();
         bool isRunning() const;
         bool started() const;
         enum { DEFAULT_PORT = 45632 };

      protected:
         void defineOptions(Poco::Util::OptionSet &options) override;
         void handleOption(const std::string &name, const std::string &value) override;
         void handleVersion(const std::string &name, const std::string &value);
         void handleFile(const std::string &name, const std::string &value);
         void handleLog(const std::string &name, const std::string &value);
         void displayHelp();
         int main(const std::vector<std::string> &args) override;

      private:
         bool _helpRequested{false};
         bool _versionRequested{false};
         bool _isRunning{true};
         bool _started{false};
         Poco::Path _configFilePath{"docstorage.properties"};
         Poco::Path _externalDbConnStringsFilePath{""};

         void initLogger(std::string const &logFilePath, Poco::Message::Priority prio, std::string const &loggerName);
      };
   }
}


