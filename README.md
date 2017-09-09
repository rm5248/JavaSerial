# Java Serial 

Useful links(note that most of the information within these links is duplicated to a smaller extent within this README):

[News]( http://programming.rm5248.com/?cat=6 )

[Project Homepage]( http://programming.rm5248.com/?page_id=19 )

[Tutorial]( http://programming.rm5248.com/?page_id=58 )

[Javadoc(latest)]( http://programming.rm5248.com/releases/JavaSerial/latest/javadoc/ )

## What is Java Serial?

Simply put, it is a project with the aim of bringing serial port reading/writing into a Java-specific format.  This means that we try to follow the Java conventions whenever possible, using `InputStream`s and `OutputStream`s to read and write data to the serial port.

## How do I use it in a project?

The easiest way is to use [Apache Maven]( http://maven.apache.org/ ), and add it as a dependency:

```xml
<dependency>
    <groupId>com.rm5248</groupId>
    <artifactId>JavaSerial</artifactId>
    <version>0.7</version>
</dependency>
```
However, you may also download the JARs and add them manually to your project if you so desire.  The latest versions may be downloaded [here]( http://programming.rm5248.com/releases/JavaSerial/latest/ ).  No other dependencies are required.

Once you have added the JAR as a dependency, you may create a new serial port object:

```java
import com.rm5248.serial.NoSuchPortException;
import com.rm5248.serial.NotASerialPortException;
import com.rm5248.serial.SerialPort;
 
public class SerialTest {
 
    public static void main(String[] args) {
        try {
            //This would be COM1, COM2, etc on Windows
            SerialPort s = new SerialPort( "/dev/ttyUSB0" );
        } catch (NoSuchPortException e) {
            System.err.println( "Oh no!  That port doesn't exist!" );
        } catch (NotASerialPortException e) {
            System.err.println( "Oh no!  That's not a serial port!" );
        }
 
    }
 
}
```

From here, you can get the `InputStream` and `OutputStream` of the object and read/write from the port.  
If you don't need to know about the serial lines at all, open up the serial port like the following:
```
new SerialPort( "/dev/ttyUSB0", SerialPort.NO_CONTROL_LINE_CHANGE );
```
Otherwise a new thread will be created for each serial port that is opened.

## JNI and Environment Variables
All of the JNI code is extracted from the JAR file and loaded at run-time, so there is no fiddling of libraries that has to be done.  If you do require special JNI code for some reason, you can set the following environment variables when starting up Java:
```
com.rm5248.javaserial.lib.path - The directory to look in for the javaserial.[dll|so]
com.rm5248.javaserial.lib.name - The name of the library to load(default:javaserial)
```
Set them like the following:
```
java -Dcom.rm5248.javaserial.lib.path=/path/to/javaserial.[dll|so|jnilib] -Dcom.rm5248.javaserial.lib.name=custom-name
```
Pre-compiled binaries are provided for:
* Windows(i586, amd64)
* Mac(amd64)
* Linux(i586, amd64, ARM)

## License
Apache 2.0

## Why a new serial port library?
First, let's go through some of the prominent serial port libraries and talk about what I feel their deficiencies are:
* [Java Comm API](http://www.oracle.com/technetwork/java/index-jsp-141752.html) - The API that most of these libraries use.  Predates the Java community process.  It's a stupid API that appears to be designed to mirror how serial ports on Windows work.
* [RXTX](http://rxtx.qbang.org/wiki/index.php/Main_Page) - the old guard.  Uses the Java Comm API.  There are a few forks, but the main branch does not seem to receive updates frequently.  The code is very complicated(>25000 lines!).  The settings that it opens the serial port with on Linux are not the best for getting raw data.
* [JSSC](https://github.com/scream3r/java-simple-serial-connector) - Has an API similar to the Java Comm API.  Doesn't provide an easy way of getting raw data from the serial port.  The code is relatively small though(<5000 lines)
* [PureJavaComm](http://www.sparetimelabs.com/purejavacomm/purejavacomm.php) - I haven't used this in the past, so I can't comment on it.

All of the above libraries have their own strengths and weaknesses.  If they use the Java Comm API, they are hobbled by the fact that it is a poorly designed API compared to the newer Java APIs.  It was also made in an era before enums, so it makes improper programming easier.
Advantages of JavaSerial:
* Small code size(~3800 total lines of code, Java/JNI)
* Auto-extracting JNI code means no worrying about native libraries for each platform.
* java.io compatible - uses standard `InputStream` and `OutputStream` to read/write data
* Raw serial port settings on Linux to get all the bytes all the time - uses the same settings as [GTKTerm](https://fedorahosted.org/gtkterm/)
* Open up any port you want as a serial port - no need to enumerate all ports before opening up a port
* Enum-based settings allow for clear programming
* No external dependencies

Disadvantages of JavaSerial:
* No locking of the serial port - There's no portable way to do this, so any locking done will be on the Java side.  This is a consequence of being rather low-level.
* ???
