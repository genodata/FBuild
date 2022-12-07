/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/*
 *
 * Author: Frank Barwich
 */

#pragma once

#include "CppDepends.h"
#include "LastWriteTime.h"

#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <thread>



class CppOutOfDate {
public:
   CppOutOfDate (const std::string& objectFileExtension) 
      : objectFileExtension_{"." + objectFileExtension}
   {
      scriptTime_ = LastWriteTime("FBuild.js");
      files_.reserve(1000);

      CppDepends::ClearIncludePath();
      CppDepends::PrecompiledHeader("");
   }

   void OutDir (std::string v)                      { outdir_ = std::move(v); }
   void Threads (uint32_t v)                        { numberOfThreads_ = v; }
   void AddIncludePath (const std::string& path)    { CppDepends::AddIncludePath(path); }
   void Files (std::vector<std::string>&& v)        { files_ = std::move(v); }
   void Files (const std::vector<std::string>& v)   { std::copy(v.begin(), v.end(), std::back_inserter(files_)); }
   void Include (const std::vector<std::string>& v) { std::for_each(v.begin(), v.end(), [] (const std::string& s) { CppDepends::AddIncludePath(s); }); }
   void PrecompiledHeader (const std::string& v)    { CppDepends::PrecompiledHeader(v); }

   void Go ()
   {
      if (outdir_.empty()) {
         throw std::runtime_error("Missing 'Outdir'");
      }

      uint32_t cpus = std::thread::hardware_concurrency();
      if (!cpus) cpus = 2;
      if (numberOfThreads_) cpus = numberOfThreads_;

      for (size_t i = 0; i < cpus; ++i) {
         threadGroup_.emplace_back(std::thread([this] () { Thread(); }));
      }

      for (auto& thread : threadGroup_) {
         thread.join();
      }
   }

   const std::vector<std::string>& OutOfDate () const { return outOfDate_; }

private:
   std::vector<std::thread> threadGroup_;
   std::mutex               outOfDateMutex_;
   std::atomic<size_t>      current_{0};
   uint64_t                 scriptTime_{0};
   std::string              objectFileExtension_;

   std::string              outdir_;
   uint32_t                 numberOfThreads_{0};
   std::vector<std::string> files_;
   std::vector<std::string> outOfDate_;

   bool GetFile (std::filesystem::path& result)
   {
      const auto index = current_++;
      if (index >= files_.size()) {
         return false;
      }

      result = std::move(files_[index]);
      return true;
   }

   bool Done ()
   {
      return current_ == files_.size();
   }

   void AddOutOfDate (const std::string& file)
   {
      std::lock_guard lock(outOfDateMutex_);
      outOfDate_.push_back(file);
   }

   void Thread ()
   {
      std::filesystem::path objdir(outdir_);
      std::filesystem::path file;

      CppDepends dep;
      for (;;) {
         if (!GetFile(file)) break;
         auto obj = objdir / file.filename();
         obj.replace_extension(objectFileExtension_);

         if (!std::filesystem::exists(obj)) AddOutOfDate(file.string());
         else if (!std::filesystem::file_size(obj)) AddOutOfDate(file.string());
         else if (LastWriteTime(obj) < scriptTime_) AddOutOfDate(file.string());
         else if (LastWriteTime(obj) < dep.Process(file)) AddOutOfDate(file.string());
      }
   }


};
