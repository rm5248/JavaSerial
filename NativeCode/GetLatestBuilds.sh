#!/bin/bash

# Linux builds
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-arm/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/arm/libjavaserial.so
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-x86_64/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/amd64/libjavaserial.so
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-x86/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/i386/libjavaserial.so

# Mac/ OSX build
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Mac/lastSuccessfulBuild/artifact/build/libjavaserial.dylib -O Mac/amd64/libjavaserial.jnilib

# Windows build
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Windows-x86/lastSuccessfulBuild/artifact/build/Release/javaserial.dll -O Windows/x86/javaserial.dll
#wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Windows-x86_64/lastSuccessfulBuild/artifact/build/Release/javaserial.dll -O Windows/amd64/javaserial.dll


# Note: this must be run from the 'NativeCode' directory
unzip linux-amd64.zip && mv libjavaserial.so Linux/amd64
unzip linux-arm64.zip && mv libjavaserial.so Linux/aarch64
unzip linux-armhf.zip && mv libjavaserial.so Linux/arm
unzip linux-x86.zip && mv libjavaserial.so Linux/i386

unzip osx-amd64.zip && mv libjavaserial.dylib Mac/amd64/libjavaserial.jnilib
unzip osx-arm64.zip && mv libjavaserial.dylib Mac/aarch64/libjavaserial.jnilib

unzip win-amd64.zip && mv javaserial.dll Windows/amd64
unzip win-x86.zip && mv javaserial.dll Windows/x86

rm *.zip
