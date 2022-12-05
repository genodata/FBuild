/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/*
 *
 * Author: Frank Barwich
 */

#pragma once

#include "Compiler.h"
#include "Librarian.h"

#include "JavaScriptHelper.h"

class JsLib {
   Compiler compiler;
   Librarian librarian;

   static duk_ret_t Constructor(duk_context* duktapeContext);
   static duk_ret_t Destructor(duk_context* duktapeContext);
   static duk_ret_t Build(duk_context* duktapeContext);
   static duk_ret_t Files(duk_context* duktapeContext);
   static duk_ret_t MPSkipFiles(duk_context* duktapeContext);
   static duk_ret_t DependencyCheck(duk_context* duktapeContext);
   static duk_ret_t CRT(duk_context* duktapeContext);
   static duk_ret_t ObjDir(duk_context* duktapeContext);
   static duk_ret_t Includes(duk_context* duktapeContext);
   static duk_ret_t Defines(duk_context* duktapeContext);
   static duk_ret_t Threads(duk_context* duktapeContext);
   static duk_ret_t CompileArgs(duk_context* duktapeContext);
   static duk_ret_t PrecompiledHeader(duk_context* duktapeContext);
   static duk_ret_t WarningLevel(duk_context* duktapeContext);
   static duk_ret_t WarningAsError(duk_context* duktapeContext);
   static duk_ret_t WarningDisable(duk_context* duktapeContext);

   static duk_ret_t Output(duk_context* duktapeContext);

   static duk_ret_t BeforeCompile(duk_context* duktapeContext);
   static duk_ret_t BeforeLink(duk_context* duktapeContext);

   static duk_ret_t Create(duk_context* duktapeContext);

public:

   static void Register(duk_context* duktapeContext);
};

