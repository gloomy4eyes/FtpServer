add_executable(ftpserver main.cpp)

target_link_libraries(ftpserver
    PRIVATE
    Poco::Foundation
    Poco::Net
    )

target_include_directories(ftpserver
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    )

target_sources(ftpserver
    PRIVATE
    FtpServerApplication.cpp
    FtpServerConnection.cpp
    )
