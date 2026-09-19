#pragma once
#include "auth/auth.hpp"
#include "auth/skStr.h"
#include <mutex>

#pragma comment(lib, "auth/library_x64.lib")
#pragma comment(lib, "auth/libcurl.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Normaliz.lib")

namespace KeyAuthClient {
    using namespace KeyAuth;

    //inline const std::string name = skCrypt("RAMZIX").decrypt();
    //inline const std::string ownerid = skCrypt("EIqqur0nYm").decrypt();
    //inline const std::string version = skCrypt("3.4").decrypt();


    inline const std::string name = skCrypt("Nithinkrishna8065's Application").decrypt();
    inline const std::string ownerid = skCrypt("mWWHGKA5h2").decrypt();
    inline const std::string version = skCrypt("1.0").decrypt();


    inline const std::string url = skCrypt("https://keyauth.win/api/1.3/").decrypt();
    inline const std::string path = skCrypt("").decrypt(); // (OPTIONAL) see tutorial here https://www.youtube.com/watch?v=I9rxt821gMk&t=1s

    inline api Internal(name, ownerid, version, url, path);

    inline std::once_flag init_once_flag;
    inline void EnsureInit() {
        std::call_once(init_once_flag, []() {
            Internal.init();
        });
    }
}
