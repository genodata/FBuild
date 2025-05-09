/*
* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/*
*
* Author: Frank Barwich
*/

#include "ToolChain.h"

#include <filesystem>
#include <iostream>


namespace ToolChain {

   static std::string toolchain;
   static std::string platform = "x86";

   static void CurrentFromEnvironment()
   {
      const char* envVersion = std::getenv("VisualStudioVersion");
      if (!envVersion) return;    

      const char* envPath = std::getenv("PATH");
      if (!envPath) return;

      std::string version = envVersion;
      toolchain = "MSVC";

      for (char ch : version) {
         if (ch != '.') {
            toolchain += ch;
         }
      }
   }

   void ToolChain(std::string_view newToolchain)
   {
      if (newToolchain == "MSVC") {
         CurrentFromEnvironment();
         if (toolchain.empty()) throw std::runtime_error("Unable to deduce MSVC-Version");
      }
      else if (newToolchain.substr(0, 4) == "MSVC") {
         ToolChain::toolchain.assign(newToolchain.begin(), newToolchain.end());
      }
      else if (newToolchain == "EMSCRIPTEN") {
         const char* emscriptenEnv = std::getenv("EMSCRIPTEN");
         if (!emscriptenEnv) throw std::runtime_error("Could not find environment variable 'EMSCRIPTEN'");

         ToolChain::toolchain.assign(newToolchain.begin(), newToolchain.end());
      }
      else {
         throw std::runtime_error("Unknown ToolChain " + std::string{newToolchain});
      }
   }

   std::string ToolChain()
   {
      if (!toolchain.empty()) return toolchain;
      CurrentFromEnvironment();
      if (!toolchain.empty()) return toolchain;

      throw std::runtime_error("Unable to deduce Toolchain");
   }

   void Platform(std::string_view newPlatform)
   {
      if (newPlatform != "x86" && newPlatform != "x64") throw std::runtime_error("Only x86 and x64 platforms are supported");
      ToolChain::platform.assign(newPlatform.begin(), newPlatform.end());
   }


   std::string Platform()
   {
      return platform;
   }

   std::string SetEnvBatchCall()
   {
      auto tchain = ToolChain();

      if (tchain.substr(0, 4) == "MSVC") {
         auto envname = ToolChain();
         envname.erase(0, 4);
         envname.insert(0, "VS");
         envname += "COMNTOOLS";

         auto commtoolsPathEnv = std::getenv(envname.c_str());
         if (!commtoolsPathEnv) {
            commtoolsPathEnv = std::getenv("VSAPPIDDIR"); // Aaaaargh. This points to the IDE-Directory. VS2017 no longer sets an COMNTOOLSxxx Env. Except if you're building from the VS2017 commandline, then COMNTOOLSxxx is set. So... Yeah...

            if (!commtoolsPathEnv) throw std::runtime_error("Environmentvariable " + envname + " not found");
         }

         auto batch = std::string{commtoolsPathEnv};
         batch += "../../VC/vcvarsall.bat";

         if (!std::filesystem::exists(batch)) {
            auto batch2 = std::string{commtoolsPathEnv};
            batch2 += "../../VC/Auxiliary/Build/vcvarsall.bat";  // New path in VS2017
            if (!std::filesystem::exists(batch2)) {
               throw std::runtime_error("Neither\n" + batch + "\nor\n" + batch2 + " does exist");
            }
            else {
               batch = batch2;
            }
         }

         batch = std::filesystem::canonical(batch).string();

         auto cmd = "\"" + batch + "\" ";

         auto bin = std::string{commtoolsPathEnv};
         bin += "../../VC/bin";

         if (platform == "x64") cmd += "amd64";
         else cmd += "amd64_x86";

         return "CALL " + cmd + " >nul";   // vcvarsall.bat spews out redundant crap -> nul    
      }
      else if (tchain == "EMSCRIPTEN") {
         const char* emscriptenEnv = std::getenv("EMSCRIPTEN");
         if (!emscriptenEnv) throw std::runtime_error("Could not find environment variable 'EMSCRIPTEN'");

         std::string batchfile = emscriptenEnv;
         for (auto&& ch : batchfile) if (ch == '/') ch = '\\';  // 1.35 writes it's Environment-Variable with Slashes. Previous Versions used Backslashes (at least the 1.34.1 that's on their webpage did).
         auto pos = batchfile.rfind('\\');      // The environment holds the path to emscripten
         pos = batchfile.rfind('\\', pos - 1);  // plus the string "\emscripten\<version>", where version ist the version, eg 1.34.1
         batchfile.erase(pos);                  // We have to erase the information after the actual path.

         batchfile += "\\emsdk_env.bat";

         if (!std::filesystem::exists(batchfile)) throw std::runtime_error(batchfile + " does not exist");

         return "CALL \"" + batchfile + "\" >nul ";
      }
      else {
         throw std::runtime_error("Unknown ToolChain " + tchain);
      }
   }

}
