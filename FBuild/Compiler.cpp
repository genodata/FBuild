/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/*
 *
 * Author: Frank Barwich
 */


#include "Compiler.h"
#include "CppOutOfDate.h"
#include "ToolChain.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <mutex>
#include <filesystem>
#include <sstream>
#include <atomic>
#include <random>





std::vector<std::string> ActualCompiler::ObjFiles (const std::string& extension)
{
   std::filesystem::path outpath{compiler.ObjDir()};

   std::vector<std::string> result{};

   for (auto&& file : compiler.Files()) {
      std::filesystem::path f{file};
      auto outfile = outpath / f.filename();
      outfile.replace_extension(extension);
      result.push_back(outfile.string());
   }

   return result;
}

std::vector<std::string> ActualCompiler::CompiledObjFiles (const std::string& extension)
{
   std::filesystem::path outpath{compiler.ObjDir()};

   std::vector<std::string> result{};

   for (auto&& file : outOfDate) {
      std::filesystem::path f{file};
      auto outfile = outpath / f.filename();
      outfile.replace_extension(extension);
      result.push_back(outfile.string());
   }

   return result;
}







void ActualCompilerVisualStudio::CheckParams ()
{
   if (compiler.ObjDir().empty()) compiler.ObjDir(compiler.Build());
}

void ActualCompilerVisualStudio::UpdateOutOfDate () 
{
   ::CppOutOfDate checker{ "obj" };
   checker.OutDir(compiler.ObjDir());
   checker.Threads(compiler.Threads());
   checker.Files(compiler.Files());
   checker.Include(compiler.Includes());
   checker.PrecompiledHeader(compiler.PrecompiledH());
   checker.Go();

   outOfDate = checker.OutOfDate();
}

bool ActualCompilerVisualStudio::NeedsRebuild ()
{
   CheckParams();
   outOfDate.clear();

   if (!compiler.DependencyCheck()) {
      outOfDate = compiler.Files();
   }
   else {
      UpdateOutOfDate();
   }

   return !outOfDate.empty();
}

void ActualCompilerVisualStudio::DeleteOutOfDateObjectFiles ()
{
   const auto files = CompiledObjFiles();

   for (auto&& file : files) {
      std::filesystem::remove(file);
   }
}

std::string ActualCompilerVisualStudio::CommandLine ()
{
   bool debug = compiler.Build() == "Debug";

   std::string command = "-nologo -c -EHsc -GF -FC -FS -Zc:inline -GS -DWIN32 -DWINDOWS ";
   if (atoi(ToolChain::ToolChain().substr(4, 2).c_str()) >= 17) {
      command += "-std:c++20 "; 
   }
   else {
      command += "-std:c++latest "; 
   }
   

   if (debug) command += "-D_DEBUG ";
   else command += "-DNDEBUG -D_USE_DETAILED_FUNCTION_NAME_IN_SOURCE_LOCATION=0 ";

   for (auto&& define : compiler.Defines()) command += "-D" + define + " ";


   if (ToolChain::Platform() == "x86") command += "-arch:SSE2 ";


   if (compiler.CRT() == "Static") command += "-MT";
   else command += "-MD";

   if (debug) command += "d";

   command += " ";


   if (debug) command += "-Od ";
   else command += "-Ox ";


   if (debug) command += "-RTC1 ";


   if (debug) command += "-Zi ";


   for (auto&& include : compiler.Includes()) command += "-I\"" + include + "\" ";


   command += "-W" + std::to_string(compiler.WarnLevel()) + " ";
   if (compiler.WarningAsError()) command += "-WX ";

   for (auto&& disabledWarning : compiler.WarningDisable()) command += "-wd" + std::to_string(disabledWarning) + " ";


   command += compiler.Args() + " ";


   std::filesystem::path out(compiler.ObjDir());
   if (!std::filesystem::exists(out)) std::filesystem::create_directories(out);

   command += "-Fo\"" + out.string() + "/\" ";


   command += "-Fp\"" + out.string() + "\"/PrecompiledHeader.pch ";


   const char* env = std::getenv("FB_COMPILER");
   if (env) command += std::string(env) + " ";

   if (debug) {
      env = std::getenv("FB_COMPILER_DEBUG");
      if (env) command += std::string(env) + " ";
   }
   else {
      env = std::getenv("FB_COMPILER_RELEASE");
      if (env) command += std::string(env) + " ";
   }

   return command;
}

void ActualCompilerVisualStudio::CompilePrecompiledHeaders ()
{
   if (outOfDate.empty()) return;
   if (compiler.PrecompiledCPP().empty()) return;

   std::filesystem::path cpp = std::filesystem::canonical(compiler.PrecompiledCPP());
   cpp.make_preferred();

   auto it = std::find_if(outOfDate.cbegin(), outOfDate.cend(), [&cpp] (const std::string& f) -> bool {
      return std::filesystem::equivalent(cpp, f);
   });

   if (it == outOfDate.cend()) return;

   outOfDate.erase(it);

   std::filesystem::path pch = std::filesystem::path(compiler.ObjDir()) / "PrecompiledHeader.pch";
   if (std::filesystem::exists(pch)) std::filesystem::remove(pch);

   std::string command =  "CL " + CommandLine();
   command += "-Yc\"" + std::filesystem::path{compiler.PrecompiledH()}.filename().string() + "\" ";
   command += cpp.string();

   std::string cmd = ToolChain::SetEnvBatchCall() + " & " + command;
   int rc = std::system(cmd.c_str());
   if (rc != 0) throw std::runtime_error("Compile Error");
}


class CLMPWorker {
private:
   std::filesystem::path objdir_;
   std::string cmdprefix_;
   std::vector<std::string> source_;
   bool failed_{ false };
   uint64_t starttime_{0};

public:
   CLMPWorker(std::string cmdprefix, std::filesystem::path objdir, std::vector<std::string> source)
      : cmdprefix_(std::move(cmdprefix))
      , objdir_(std::move(objdir))
      , source_(std::move(source))
      , starttime_(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count())
   {
   }

   void Wait()
   {
      failed_ = false;
      const auto path = [&] {
         std::stringstream ss;
         ss << "FBuild_CLMPWorker_" << std::this_thread::get_id() << ".rsp";
         return objdir_ / std::filesystem::path(ss.str());
      } ();

      try {
         {
            std::ofstream message(path, std::ios::trunc);
            for (const auto& src : source_) {
               message << "\"" << src << "\" ";
            }
         }

         const auto cmd = cmdprefix_ + " @" + path.string();
         const int rc = std::system(cmd.c_str());
         if (rc != 0) {
            failed_ = true;
         }
      }
      catch (std::exception& e) {
         std::cerr << e.what() << std::endl;
         failed_ = true;
      }
      catch (...) {
         failed_ = true;
      }

      std::error_code nothrow;
      std::filesystem::remove(path, nothrow);
   }

   void UpdateSourceFiles () 
   {
      source_.erase(std::remove_if(source_.begin(), source_.end(), [&](const auto& cppfile) {
         const auto check = objdir_ / std::filesystem::path(cppfile).filename().replace_extension(".obj");
         return std::filesystem::exists(check) && LastWriteTime(check) >= starttime_;
      }), source_.end());

      static std::random_device rd;
      static std::mt19937 g(rd());
      std::shuffle(source_.begin(), source_.end(), g);
   }

   bool IsFailed () const
   {
      return failed_;
   }

   const std::vector<std::string>& GetSourceFiles () const 
   {
      return source_;
   }
};



void ActualCompilerVisualStudio::CompileFiles ()
{
   if (outOfDate.empty()) {
      return;
   }

   std::string commandLine = CommandLine();

   if (compiler.PrecompiledH().size()) {
      commandLine += "-FI\"" + compiler.PrecompiledH() + "\" ";
      commandLine += "-Yu\"" + compiler.PrecompiledH() + "\" ";
   }

   const auto threads = std::clamp(compiler.Threads(), std::max(static_cast<int>(std::thread::hardware_concurrency()), 2), static_cast<int>(outOfDate.size()));

   std::vector<std::string> skipped;
   bool failed = false;

   if (!compiler.MPSkipFiles().empty()) {
      auto mpskip = compiler.MPSkipFiles();
      const auto it = std::stable_partition(outOfDate.begin(), outOfDate.end(), [&mpskip](const auto& what) {
         const auto it = std::find(mpskip.begin(), mpskip.end(), what);
         if (it != mpskip.end()) {
            mpskip.erase(it);
            return false;
         }
         return true;
      });
      skipped.insert(skipped.end(), it, outOfDate.end());
      outOfDate.erase(it, outOfDate.end());
   }

   {
      const auto cmdprefix = ToolChain::SetEnvBatchCall() + " & CL " + commandLine + " -MP" + std::to_string(threads) + " ";
      auto worker = CLMPWorker{cmdprefix, compiler.ObjDir(), outOfDate};
      worker.Wait();

      if (worker.IsFailed()) {
         worker.UpdateSourceFiles();
         const auto& files = worker.GetSourceFiles();
         skipped.insert(skipped.end(), files.begin(), files.end());
         failed = true;
      }
   }

   if (skipped.empty()) {
      return;
   }

   std::cout << std::endl;

   std::mutex mutex;
   std::atomic<int> errors;

   auto threadFunction = [&] () {
      std::string cpp;
      std::string command;

      for (;;) {
         try {
            {
               std::lock_guard<std::mutex> lock{mutex};
               auto it = skipped.rbegin();
               if (it == skipped.rend()) break;
               cpp = (*it);
               skipped.pop_back();
            }

            command = "CL " + commandLine + " -MP1 \"" + cpp + "\" ";

            std::string cmd = ToolChain::SetEnvBatchCall() + " & " + command;
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
               std::lock_guard<std::mutex> lock{mutex};
               ++errors;
            }
         }
         catch (std::exception& e) {
            std::cout << e.what() << std::endl;
            std::lock_guard<std::mutex> lock{mutex};
            ++errors;
         }
         catch (...) {
            std::lock_guard<std::mutex> lock{mutex};
            ++errors;
         }
      }
   };

   std::vector<std::thread> threadGroup;
   for (int i = 0; i < threads; ++i) {
      threadGroup.push_back(std::thread{ threadFunction });
   }

   for (auto&& thread : threadGroup) {
      thread.join();
   }

   if (errors) {
      throw std::runtime_error("Compile Error");
   }

   if (failed) {
      // Workaround für Features die nicht mit Multithreading (`-MP` > 1) kompatibel sind
      std::cerr << "\n"
         << "Workaround fuer Features die nicht mit Multithreading (`-MP` > 1) kompatibel sind aktiviert!\n"
         << "Um zukuenftige Fehler zu vermeiden die zuvor fehlgeschlagenen Quellcode Dateien in FBuild *zusätzlich* mittels `MPSkipFiles(Glob('file.cpp'))` übergeben.\n"
         << "\n\n" << std::endl;
   }
}

void ActualCompilerVisualStudio::Compile ()
{
   CheckParams();
   if (!NeedsRebuild()) return;

   std::cout << "\nCompiling (" << ToolChain::ToolChain() << " " << ToolChain::Platform() << ")" << std::endl;

   compiler.DoBeforeCompile();

   DeleteOutOfDateObjectFiles();
   CompilePrecompiledHeaders();
   CompileFiles();
}







void Compiler::Compile ()
{
   const auto toolChain = ToolChain::ToolChain();
   if (toolChain.substr(0, 4) == "MSVC") actualCompiler.reset(new ActualCompilerVisualStudio{*this});
   else throw std::runtime_error("Unbekannte Toolchain: " + toolChain);

   actualCompiler->Compile();
}

