#!/bin/bash

# Linux builds
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-arm/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/arm/libjavaserial.so
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-x86_64/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/amd64/libjavaserial.so
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Linux-x86/lastSuccessfulBuild/artifact/build/libjavaserial.so -O Linux/i386/libjavaserial.so

# Mac/ OSX build
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Mac/lastSuccessfulBuild/artifact/build/libjavaserial.dylib -O Mac/amd64/libjavaserial.jnilib

# Windows build
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Windows-x86/lastSuccessfulBuild/artifact/build/Release/javaserial.dll -O Windows/x86/javaserial.dll
wget https://jenkins.rm5248.com/view/JavaSerial/job/NativeCode-Windows-x86_64/lastSuccessfulBuild/artifact/build/Release/javaserial.dll -O Windows/amd64/javaserial.dll

