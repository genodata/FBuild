/*
* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/*
*
* Author: Frank Barwich
*/

#pragma once

#include <string>

#include <boost/utility/string_ref.hpp>

namespace ToolChain {

   void        ToolChain(boost::string_ref toolchain);
   std::string ToolChain();

   void        Platform(boost::string_ref platform);
   std::string Platform();

   std::string SetEnvBatchCall();
}

