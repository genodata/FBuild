
if Build == nil then
   Build = "Release" 
end

if Build ~= "Release" and Build ~= "Debug" then 
   error("Build='<Release|Debug>' expected");
end;

local dep = {
   Outdir      = Build;  
   Includes    = { "/boost_1_49_0" };
   Files       = FBuild.Glob("*.cpp");
};

local outOfDate = FBuild.CppOutOfDate(dep);

local compileOptions = {
   Config            = Build;                
   Outdir            = Build; 
   Includes          = { "/boost_1_49_0" };
   CRT               = 'Static';
   Files             = outOfDate;
   PrecompiledHeader = {         
      Header = 'Precompiled.h';  
      Cpp    = 'Precompiled.cpp';
   };
};


FBuild.Compile(compileOptions); 


local linkOptions = {
   Config = Build;                
   Output = "../" .. Build .. "/FBuild.exe";
   Libpath = { "/boost_1_49_0/stage/lib", "../" .. Build };
   Libs = { 
      "Lua.lib",
      "Shlwapi.lib",
   };
   Files = FBuild.Glob(Build, "*.obj");
   
};

if FBuild.FileOutOfDate(linkOptions.Output, linkOptions.Files) then
   FBuild.Link(linkOptions); 
end                      

