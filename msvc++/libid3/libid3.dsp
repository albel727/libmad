# Microsoft Developer Studio Project File - Name="libid3" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libid3 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libid3.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libid3.mak" CFG="libid3 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libid3 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libid3 - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libid3 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /GX /O2 /I "." /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libid3 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /Gm /GX /ZI /Od /I "." /D "_LIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "DEBUG" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libid3 - Win32 Release"
# Name "libid3 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\libid3\compat.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\crc.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\debug.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\field.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\file.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\frame.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\frametype.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\latin1.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\parse.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\render.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\tag.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\ucs4.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\utf16.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\utf8.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\util.c
# End Source File
# Begin Source File

SOURCE=..\..\libid3\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\libid3\compat.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\crc.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\debug.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\field.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\file.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\frame.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\frametype.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\global.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\id3.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\latin1.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\parse.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\render.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\tag.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\ucs4.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\utf16.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\utf8.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\util.h
# End Source File
# Begin Source File

SOURCE=..\..\libid3\version.h
# End Source File
# End Group
# Begin Group "Data Files"

# PROP Default_Filter "dat"
# Begin Source File

SOURCE=..\..\libid3\genre.dat
# End Source File
# End Group
# End Target
# End Project
