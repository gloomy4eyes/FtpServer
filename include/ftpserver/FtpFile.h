#pragma once

namespace ftp {
    namespace server {

        class FtpFile {
        public:
            std::string path() const = 0;
            std::string parent() const = 0;

            bool isFolder() const = 0;
            bool isFile() const = 0;

        };

    }
}