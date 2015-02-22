# Microsoft Developer Studio Project File - Name="stlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=stlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "stlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "stlib.mak" CFG="stlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "stlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "stlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "stlib - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x40d /d "NDEBUG"
# ADD RSC /l 0x40d /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "stlib - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x40d /d "_DEBUG"
# ADD RSC /l 0x40d /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "stlib - Win32 Release"
# Name "stlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\8svx.c
# End Source File
# Begin Source File

SOURCE=..\src\adpcm.c
# End Source File
# Begin Source File

SOURCE=..\src\adpcm.h
# End Source File
# Begin Source File

SOURCE=..\src\aiff.c
# End Source File
# Begin Source File

SOURCE=..\src\alsa.c
# End Source File
# Begin Source File

SOURCE=..\src\au.c
# End Source File
# Begin Source File

SOURCE=..\src\auto.c
# End Source File
# Begin Source File

SOURCE=..\src\avg.c
# End Source File
# Begin Source File

SOURCE=..\src\avr.c
# End Source File
# Begin Source File

SOURCE=..\src\band.c
# End Source File
# Begin Source File

SOURCE=..\src\bandpass.c
# End Source File
# Begin Source File

SOURCE=..\src\breject.c
# End Source File
# Begin Source File

SOURCE=..\src\btrworth.c
# End Source File
# Begin Source File

SOURCE=..\src\cdr.c
# End Source File
# Begin Source File

SOURCE=..\src\chorus.c
# End Source File
# Begin Source File

SOURCE=..\src\compand.c
# End Source File
# Begin Source File

SOURCE=..\src\copy.c
# End Source File
# Begin Source File

SOURCE=..\src\cvsd.c
# End Source File
# Begin Source File

SOURCE=..\src\dat.c
# End Source File
# Begin Source File

SOURCE=..\src\dcshift.c
# End Source File
# Begin Source File

SOURCE=..\src\deemphas.c
# End Source File
# Begin Source File

SOURCE=..\src\earwax.c
# End Source File
# Begin Source File

SOURCE=..\src\echo.c
# End Source File
# Begin Source File

SOURCE=..\src\echos.c
# End Source File
# Begin Source File

SOURCE=..\src\FFT.c
# End Source File
# Begin Source File

SOURCE=..\src\fade.c
# End Source File
# Begin Source File

SOURCE=..\src\filter.c
# End Source File
# Begin Source File

SOURCE=..\src\flanger.c
# End Source File
# Begin Source File

SOURCE=..\src\g711.c
# End Source File
# Begin Source File

SOURCE=..\src\g711.h
# End Source File
# Begin Source File

SOURCE=..\src\g721.c
# End Source File
# Begin Source File

SOURCE=..\src\g723_16.c
# End Source File
# Begin Source File

SOURCE=..\src\g723_24.c
# End Source File
# Begin Source File

SOURCE=..\src\g723_40.c
# End Source File
# Begin Source File

SOURCE=..\src\g72x.c
# End Source File
# Begin Source File

SOURCE=..\src\getopt.c
# End Source File
# Begin Source File

SOURCE=..\src\gsm.c
# End Source File
# Begin Source File

SOURCE=..\src\handlers.c
# End Source File
# Begin Source File

SOURCE=..\src\hcom.c
# End Source File
# Begin Source File

SOURCE=..\src\highp.c
# End Source File
# Begin Source File

SOURCE=..\src\highpass.c
# End Source File
# Begin Source File

SOURCE=..\src\ima_rw.c
# End Source File
# Begin Source File

SOURCE=..\src\lowp.c
# End Source File
# Begin Source File

SOURCE=..\src\lowpass.c
# End Source File
# Begin Source File

SOURCE=..\src\mask.c
# End Source File
# Begin Source File

SOURCE=..\src\maud.c
# End Source File
# Begin Source File

SOURCE=..\src\mcompand.c
# End Source File
# Begin Source File

SOURCE=..\src\misc.c
# End Source File
# Begin Source File

SOURCE=..\src\mp3.c
# End Source File
# Begin Source File

SOURCE=..\src\noiseprof.c
# End Source File
# Begin Source File

SOURCE=..\src\noisered.c
# End Source File
# Begin Source File

SOURCE=..\src\nulfile.c
# End Source File
# Begin Source File

SOURCE=..\src\oss.c
# End Source File
# Begin Source File

SOURCE=..\src\pan.c
# End Source File
# Begin Source File

SOURCE=..\src\phaser.c
# End Source File
# Begin Source File

SOURCE=..\src\pitch.c
# End Source File
# Begin Source File

SOURCE=..\src\polyphas.c
# End Source File
# Begin Source File

SOURCE=..\src\prc.c
# End Source File
# Begin Source File

SOURCE=..\src\rate.c
# End Source File
# Begin Source File

SOURCE=..\src\raw.c
# End Source File
# Begin Source File

SOURCE=..\src\repeat.c
# End Source File
# Begin Source File

SOURCE=..\src\resample.c
# End Source File
# Begin Source File

SOURCE=..\src\reverb.c
# End Source File
# Begin Source File

SOURCE=..\src\reverse.c
# End Source File
# Begin Source File

SOURCE=..\src\sf.c
# End Source File
# Begin Source File

SOURCE=..\src\silence.c
# End Source File
# Begin Source File

SOURCE=..\src\skeleff.c
# End Source File
# Begin Source File

SOURCE=..\src\smp.c
# End Source File
# Begin Source File

SOURCE=..\src\sndrtool.c
# End Source File
# Begin Source File

SOURCE=..\src\speed.c
# End Source File
# Begin Source File

SOURCE=..\src\sphere.c
# End Source File
# Begin Source File

SOURCE=..\src\stat.c
# End Source File
# Begin Source File
#
SOURCE=..\src\stio.c
# End Source File
# Begin Source File

SOURCE=..\src\stretch.c
# End Source File
# Begin Source File

SOURCE=..\src\sunaudio.c
# End Source File
# Begin Source File

SOURCE=..\src\swap.c
# End Source File
# Begin Source File

SOURCE=..\src\synth.c
# End Source File
# Begin Source File

SOURCE=..\src\trim.c
# End Source File
# Begin Source File

SOURCE=..\src\tx16w.c
# End Source File
# Begin Source File

SOURCE=..\src\util.c
# End Source File
# Begin Source File

SOURCE=..\src\vibro.c
# End Source File
# Begin Source File

SOURCE=..\src\voc.c
# End Source File
# Begin Source File

SOURCE=..\src\vol.c
# End Source File
# Begin Source File

SOURCE=..\src\vorbis.c
# End Source File
# Begin Source File

SOURCE=..\src\vox.c
# End Source File
# Begin Source File

SOURCE=..\src\wav.c
# End Source File
# Begin Source File

SOURCE=..\src\wve.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\btrworth.h
# End Source File
# Begin Source File

SOURCE=..\src\cvsdfilt.h
# End Source File
# Begin Source File

SOURCE=..\src\FFT.h
# End Source File
# Begin Source File

SOURCE=..\src\g72x.h
# End Source File
# Begin Source File

SOURCE=..\src\ima_rw.h
# End Source File
# Begin Source File

SOURCE=..\src\noisered.h
# End Source File
# Begin Source File

SOURCE=..\src\resampl.h
# End Source File
# Begin Source File

SOURCE=..\src\sfircam.h
# End Source File
# Begin Source File

SOURCE=..\src\st.h
# End Source File
# Begin Source File

SOURCE=..\src\st_i.h
# End Source File
# Begin Source File

SOURCE=.\ststdint.h
# End Source File
# Begin Source File

SOURCE=..\src\wav.h
# End Source File
# End Group
# End Target
# End Project
