#
# Created by gmakemake x86 (Feb 24 2012) on Sat Aug  4 19:31:04 2012
#

#
# Definitions
#

.SUFFIXES:
.SUFFIXES:	.a .o .c .C .cpp .s .S
.c.o:
		$(COMPILE.c) $< -o $@
.C.o:
		$(COMPILE.cc) $< -o $@
.cpp.o:
		$(COMPILE.cc) $< -o $@
.S.s:
		$(CPP) -o $*.s $<
.s.o:
		$(COMPILE.cc) $< -o $@
.c.a:
		$(COMPILE.c) -o $% $<
		$(AR) $(ARFLAGS) $@ $%
.C.a:
		$(COMPILE.cc) -o $% $<
		$(AR) $(ARFLAGS) $@ $%
.cpp.a:
		$(COMPILE.cc) -o $% $<
		$(AR) $(ARFLAGS) $@ $%

CC = 		gcc
CXX = 		g++

RM = rm -f
AR = ar
LINK.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)
LINK.cc = $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)
COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) -c
COMPILE.cc = $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c

########## Default flags (redefine these with a header.mak file if desired)
CPPFLAGS = 	-DPIC -I$(JNI_INCLUDE_DIR) -I$(JNI_INCLUDE_DIR)/linux
CXXFLAGS =	
CFLAGS =	-fPIC -Wall $(ARCH)
CLIBFLAGS =	-lm 
CCLIBFLAGS =	
########## End of default flags


CPP_FILES =	
C_FILES =	SerialPortImpl.c
PS_FILES = 	 
S_FILES =	
H_FILES =	com_rm5248_serial_SerialPort.h com_rm5248_serial_SerialPort_SerialInputStream.h com_rm5248_serial_SerialPort_SerialOutputStream.h
SOURCEFILES =	$(H_FILES) $(CPP_FILES) $(C_FILES) $(S_FILES)
.PRECIOUS:	$(SOURCEFILES)
OBJFILES =	SerialPortImpl.o 

JNI_INCLUDE_DIR = /usr/lib/jvm/java-7-openjdk-amd64/include
JNI_INCLUDE_DIR_WIN=C:\Program Files (x86)\Java\jdk1.6.0_18\include

#
# Main targets
#
linux: SerialPortImpl.o
	$(CC) -o libjavaserial.so $(ARCH) -shared -Wl,-soname,libjavaserial.so -I$(JNI_INCLUDE_DIR) -I$(JNI_INCLUDE_DIR)/linux -lc $(OBJFILES)

linux-32:
	$(MAKE) ARCH="-m32"
linux-64:
	$(MAKE) ARCH="-m64"

windows:
	cl -I"$(JNI_INCLUDE_DIR_WIN)" -I"$(JNI_INCLUDE_DIR_WIN)\win32" -I"." -MT -LD SerialPortImpl.c -Fejavaserial.dll
#
# Dependencies
#

SerialPortImpl.o: SerialPortImpl.c com_rm5248_serial_SerialPort.h \
 com_rm5248_serial_SerialInputStream.h \
 com_rm5248_serial_SerialOutputStream.h \
 com_rm5248_serial_SimpleSerialInputStream.h

#
# Installation Targets
#

install: 
	/usr/bin/install /usr/bin

#
# Housekeeping
#

Archive:	archive.tgz

archive.tgz:	$(SOURCEFILES) Makefile
	tar cf - $(SOURCEFILES) Makefile | gzip > archive.tgz

clean:
	-/bin/rm $(OBJFILES) core 2> /dev/null

realclean:        clean
	-/bin/rm -rf 
