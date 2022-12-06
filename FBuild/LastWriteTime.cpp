#include "LastWriteTime.h"

#include <optional>
#include <unordered_map>
#include <fstream>
#include <string>
#include <cctype>
#include <array>
#include <iostream>

#include "PicoSHA2/picosha2.h"

class Cache {

   struct PersistentValue {
      uint64_t ts{};
      std::string hash;
   };

   struct PersistentStorageRecord {
      std::filesystem::path file;
      PersistentValue value;
   };

   friend std::istream& operator>> (std::istream& stream, PersistentStorageRecord& output) 
   {
      stream >> output.file;

      stream >> output.value.ts;

      stream >> output.value.hash;
      
      return stream;
   }

   friend std::ostream& operator<< (std::ostream& stream, const PersistentStorageRecord& input)
   {
      stream << input.file << " ";

      stream << input.value.ts << " ";

      stream << input.value.hash << "\n";

      return stream;
   }



   std::mutex persistentMutex_;
   std::unordered_map<std::filesystem::path, PersistentValue> persistentCache_;
   bool persistentChanged_{false};

   std::mutex lastWriteTimeMutex_;
   std::unordered_map<std::filesystem::path, uint64_t> lastWriteTimeCache_;



   uint64_t QueryFileTime (const std::filesystem::path& file)
   {
      uint64_t result{};

      try {
         result = std::chrono::duration_cast<std::chrono::seconds>(std::filesystem::last_write_time(file).time_since_epoch()).count();
      }
      catch (...) {
      }

      return result;
   }

   std::string QueryFileHash (const std::filesystem::path& file) 
   {
      std::string result;

      try {
         std::array<unsigned char, picosha2::k_digest_size> bytes;
         std::ifstream stream(file, std::ios::binary);
         picosha2::hash256(stream, bytes.begin(), bytes.end());
         picosha2::bytes_to_hex_string(bytes.begin(), bytes.end(), result);
      }
      catch (...) {
      }

      return result;
   }

   void UpdateCache (const std::filesystem::path& file, PersistentValue& stored) 
   {
      const auto ctime = QueryFileTime(file);
      if (ctime > stored.ts) {
         auto hash = QueryFileHash(file);
         if (stored.hash != hash) {
            stored.hash = std::move(hash);
         }
         stored.ts = ctime;
         persistentChanged_ = true;
      }
   }

   static bool Skip (std::string extension) 
   {
      std::transform(extension.begin(), extension.end(), extension.begin(),[](unsigned char c) { 
         return static_cast<unsigned char>(std::tolower(c)); 
      });

      return extension != ".h" 
         && extension != ".c"
         && extension != ".hpp"
         && extension != ".cpp" 
         && extension != ".cxx"
         && extension != ".rc";
   }

   uint64_t QueryPersistent (const std::filesystem::path& file) 
   {
      try {
         if (Skip(file.extension().string())) {
            return QueryFileTime(file);
         }

         const auto normalized = std::filesystem::canonical(file);
         const auto lock = std::lock_guard{ persistentMutex_ };
         const auto it = persistentCache_.find(normalized);

         if (it != end(persistentCache_)) {
            UpdateCache(normalized, it->second);
            return it->second.ts;
         }

         persistentChanged_ = true;
         return persistentCache_.insert(std::make_pair(normalized, PersistentValue{ QueryFileTime(normalized), QueryFileHash(normalized) })).first->second.ts;
      }
      catch (...) {
      }

      return 0;
   }

   std::optional<uint64_t> QueryCacheTime (const std::filesystem::path& file) 
   {
      std::optional<uint64_t> result{};     

      const auto lock = std::lock_guard{lastWriteTimeMutex_};
      const auto it = lastWriteTimeCache_.find(file);
      if (it != end(lastWriteTimeCache_)) {
         result = it->second;
      }

      return result;
   }

   void UpdateCache (const std::filesystem::path& file, uint64_t value) 
   {
      const auto lock = std::lock_guard{lastWriteTimeMutex_};
      lastWriteTimeCache_.insert(std::make_pair(file, value));
   }

   static std::filesystem::path CacheFile() 
   {
      return std::filesystem::temp_directory_path() / "FBuild_TimestampCache_v1.txt";
   }

   static std::unordered_map<std::filesystem::path, PersistentValue> LoadCacheFile ()
   {
      std::unordered_map<std::filesystem::path, PersistentValue> result;

      try {
         std::vector<char> iobuffer(4096 * 16, '\0');
         std::ifstream stream(CacheFile());
         stream.rdbuf()->pubsetbuf(iobuffer.data(), iobuffer.size());

         PersistentStorageRecord record;
         while (stream >> record) {
            if (Skip(record.file.extension().string())) {
               continue;
            }
            result.insert(std::make_pair(record.file, record.value));
         }
      }
      catch (std::exception& e) {
         std::cerr << "FBuild: " << CacheFile() << ": " << e.what() << "\n";
      }

      return result;
   }

   void SaveCacheFile() 
   {
      try {
         if (persistentChanged_) {
            { // merge
               auto meanwhile = LoadCacheFile();
               for (auto&& item : meanwhile) {
                  const auto it = persistentCache_.find(item.first);
                  if (it != end(persistentCache_) && item.second.ts > it->second.ts) {
                     it->second = std::move(item.second);
                  }
               }
            }

            std::vector<char> iobuffer(4096 * 16, '\0');
            std::ofstream stream(CacheFile(), std::ios::trunc);
            stream.rdbuf()->pubsetbuf(iobuffer.data(), iobuffer.size());

            for (auto&& item : persistentCache_) {
               stream << PersistentStorageRecord{item.first, std::move(item.second)};
            }
         }
      }
      catch (std::exception& e) {
         std::cerr << "FBuild: " << CacheFile() << ": " << e.what() << "\n";
      }
   }

public:
   Cache () : persistentCache_(LoadCacheFile())
   {
   }

   ~Cache () 
   {
      SaveCacheFile();
   }

   
   uint64_t LastWriteTime (std::filesystem::path file) 
   {
      if (const auto cache = QueryCacheTime(file); cache) {
         return *cache;
      }

      const auto actual = QueryPersistent(file);
      UpdateCache(file, actual);

      return actual;
   }
};









uint64_t LastWriteTime (const std::filesystem::path& file)
{
   static auto cache = Cache{};
   return cache.LastWriteTime(file);
}
