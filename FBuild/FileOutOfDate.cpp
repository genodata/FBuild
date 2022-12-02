/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/*
 *
 * Author: Frank Barwich
 */

#include "FileOutOfDate.h"
#include "LastWriteTime.h"

#include <filesystem>


bool FileOutOfDate::Go () const
{
   const auto parentTime = LastWriteTime(parent);
   if (!parentTime) {
      return true;
   }

   for (size_t i = 0; i < files.size(); ++i) {
      if (LastWriteTime(files[i]) > parentTime) {
         return true;
      }
   }

   return false;
}
