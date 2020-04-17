if (NOT TARGET Poco::Foundation)
    find_package(Poco COMPONENTS Foundation Net REQUIRED)
endif ()