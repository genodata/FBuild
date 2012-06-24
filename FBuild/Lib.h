/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/*
 *
 * Author: Frank Barwich
 */

#pragma once

#include <string>
#include <vector>

class Lib {
   std::string              output;
   std::vector<std::string> files;

public:

   void Output (const std::string& v)        { output = v; }
   void Files (std::vector<std::string>&& v) { files = std::move(v); }

   void Go ();
};

