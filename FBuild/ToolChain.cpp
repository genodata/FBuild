/*
* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/*
*
* Author: Frank Barwich
*/

#include "ToolChain.h"

#include <boost/filesystem.hpp>


namespace ToolChain {

   static std::string toolchain;
   static std::string platform;

   static void CurrentFromEnvironment()
   {
      const char* envVersion = std::getenv("VisualStudioVersion");
      if (!envVersion) return;

      const char* envPath = std::getenv("PATH");
      if (!envPath) return;

      std::string version = envVersion;
      toolchain = "MSVC";

      for (char ch : version) if (ch != '.') toolchain += ch;

      std::string path = envPath;
      
      if (path.find("\\VC\\BIN\\x86_amd64") != std::string::npos || path.find("\\VC\\BIN\\amd64") != std::string::npos) platform = "x64";
      else platform = "x86";
   }

   static void LatestInstalledVersion()
   {
      std::string envString;
      for (size_t i = 20; i >= 8; --i) {
         envString = "VS" + std::to_string(i) + "0COMNTOOLS";

         const char* env = std::getenv(envString.c_str());
         if (env) {
            toolchain = "MSVC" + std::to_string(i) + "0";
            if (platform.empty()) platform = "x86";
            break;
         }
      }
   }
   

   void ToolChain(boost::string_ref toolchain)
   {
      if (toolchain.substr(0, 4) != "MSVC") throw std::runtime_error("Unbekannte Toolchain " + toolchain.to_string());
      
      std::string envname = toolchain.to_string();
      envname.erase(0, 4);
      envname.insert(0, "VS");
      envname += "COMNTOOLS";

      const char* commtoolsPathEnv = std::getenv(envname.c_str());
      if (!commtoolsPathEnv) throw std::runtime_error("Invalid ToolChain '" + toolchain.to_string() + "' " + envname + " not found");

      ToolChain::toolchain.assign(toolchain.begin(), toolchain.end());
   }

   std::string ToolChain()
   {
      if (toolchain.size()) return toolchain;
      CurrentFromEnvironment();
      if (toolchain.size()) return toolchain;
      LatestInstalledVersion();
      if (toolchain.size()) return toolchain;

      throw std::runtime_error("Unable to deduce Toolchain");
   }

   void Platform(boost::string_ref platform)
   {
      if (platform != "x86" && platform != "x64") throw std::runtime_error("Only x86 and x64 platforms are supported");
      ToolChain::platform.assign(platform.begin(), platform.end());
   }


   std::string Platform()
   {
      if (platform.size()) return platform;
      CurrentFromEnvironment();
      if (platform.size()) return platform;
      LatestInstalledVersion();
      if (platform.size()) return platform;

      throw std::runtime_error("Unable to deduce Toolchain");
   }

   std::string SetEnvBatchCall()
   {
      std::string envname = ToolChain();
      envname.erase(0, 4);
      envname.insert(0, "VS");
      envname += "COMNTOOLS";

      const char* commtoolsPathEnv = std::getenv(envname.c_str());
      if (!commtoolsPathEnv) throw std::runtime_error("Environmentvariable " + envname + " not found");

      std::string batch = commtoolsPathEnv;
      batch += "../../VC/vcvarsall.bat";

      batch = boost::filesystem::canonical(batch).string();
      if (!boost::filesystem::exists(batch)) throw std::runtime_error(batch + " does not exist");

      std::string cmd = "\"" + batch + "\" ";

      if (platform == "x64") cmd += "x86_amd64";
      else cmd += "x86";

      std::cout << "Batch:" << cmd << std::endl;

      return "CALL " + cmd;
   }
}
