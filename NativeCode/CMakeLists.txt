PROJECT(javaserial LANGUAGES C)

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

find_package(JNI)

if ( NOT JNI_FOUND)
    message( FATAL_ERROR "No JNI found, can't compile" )
else()
    message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
    message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
endif()

INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})

# Source Files
SET(JAVASERIAL_SOURCES SerialPortImpl.c)
SET(JAVASERIAL_HEADERS com_rm5248_serial_SerialInputStream.h
   com_rm5248_serial_SerialPort.h
   com_rm5248_serial_SerialOutputStream.h
   com_rm5248_serial_SimpleSerialInputStream.h
)

ADD_LIBRARY( javaserial SHARED ${JAVASERIAL_SOURCES} )
