#include "ftpserver/FtpServerApplication.h"

int main(int argc, char *argv[]) {

#ifdef _WIN32
  setlocale(LC_ALL, "Russian.1251");
#endif

  swf::docstorage::MainApplication app;
  return app.run(argc, argv);
}