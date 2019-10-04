package com.rm5248.serial;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * A SerialPort represents a serial port on the system.
 *
 * To open a serial port, use the appropriate constructor to open it, providing
 * the entire path to the serial port, and optionally settings. You can define:
 *
 * <ul>
 * <li>The speed of the serial port
 * <li>The number of data bits
 * <li>The number of stop bits.
 * <li>The parity
 * <li>The flow control
 * <li>The control lines to monitor
 * </ul>
 * The special parameter controlLineFlags to the constructor defines what
 * control lines you want to listen for. By default, you will get notifications
 * for {@link ALL_CONTROL_LINES} if you install a {@link SerialChangeListener}.
 * Note that any control line changes you want to listen for require a new
 * thread per serial port. Often you don't need to listen to any serial line
 * changes, and in that case you can pass {@link NO_CONTROL_LINE_CHANGE} to the
 * constructor.
 *
 * <p>
 * Example:
 * <pre>
 * {@code
 * //Open up a serial port on Mac/Linux
 * SerialPort s = new SerialPort( "/dev/ttyS0" );
 * //Open up a serial port on Windows
 * SerialPort s = new SerialPort( "COM1" );
 * }
 * </pre>
 *
 * Once the port has been opened, you can change any setting that you want
 * to(baud, parity, etc).
 * <p>
 * The native code is automatically extracted from the jar file. If you need to
 * use a special version, there are two Java properties to set.
 *
 * <p>
 *
 * {@code com.rm5248.javaserial.lib.path} - give the directory name that the JNI code is
 * located in <br/>
 * {@code com.rm5248.javaserial.lib.name} - explicitly give the name of the
 * library(the default is 'javaserial')
 * </pre>
 *
 *
 * @author rm5248
 *
 */
public class SerialPort implements AutoCloseable {

    private final static Logger logger = Logger.getLogger( SerialPort.class.getName() );
    /* Native logger.  Don't remove this! The JNI needs to access it */
    private final static Logger native_logger = Logger.getLogger( SerialPort.class.getName() + ".native" );

    //The first time this class is referenced, we need to load the library.
    static{
        loadNativeLibrary();
    }

    /**
     * Load the native library.
     *
     * There are two important system properties that can be set here:
     *
     * com.rm5248.javaserial.lib.path - give the directory name that the JNI
     * code is located in com.rm5248.javaserial.lib.name - explicitly give the
     * name of the library(the default is 'javaserial')
     *
     * This is based largely off of SQLite-JDBC(
     * https://github.com/xerial/sqlite-jdbc )
     */
    private static void loadNativeLibrary(){
        String nativeLibraryPath = System.getProperty( "com.rm5248.javaserial.lib.path" );
        String nativeLibraryName = System.getProperty( "com.rm5248.javaserial.lib.name" );

        if( nativeLibraryName == null ){
            nativeLibraryName = System.mapLibraryName( "javaserial" );
            if( nativeLibraryName.endsWith( "dylib" ) ){
                //mac uses jnilib instead of dylib for some reason
                nativeLibraryName = nativeLibraryName.replace( "dylib", "jnilib" );
            }
            logger.log( Level.FINE, "No native library name provided, using default of {0}",
                    nativeLibraryName);
        }

        if( nativeLibraryPath != null ){
            logger.log( Level.FINE, "Native library path of {0} provided", nativeLibraryPath );
            File libToLoad = new File( nativeLibraryPath, nativeLibraryName );
            logger.log( Level.FINE, "Loading library {0}", libToLoad.getAbsolutePath() );
            System.load( libToLoad.getAbsolutePath() );
            return;
        }

        //if we get here, that means that we must extract the JNI from the jar
        try{
            File extractedLib;
            Path tempFolder;
            String osName;
            String arch;
            InputStream library;

            osName = System.getProperty( "os.name" );
            if( osName.contains( "Windows" ) ){
                osName = "Windows";
            } else if( osName.contains( "Mac" ) || osName.contains( "Darwin" ) ){
                osName = "Mac";
            } else if( osName.contains( "Linux" ) ){
                osName = "Linux";
            } else{
                osName = osName.replaceAll( "\\W", "" );
            }

            arch = System.getProperty( "os.arch" );
            arch.replaceAll( "\\W", "" );

            if( arch.equals( "x86_64" ) ){
                //map x86_64 to amd64 to stay consistent(needed for mac)
                arch = "amd64";
            }

            //create the temp folder to extract the library to
            tempFolder = Files.createTempDirectory( "javaserial" );
            tempFolder.toFile().deleteOnExit();

            logger.log( Level.FINER, "Created temp folder of {0}", tempFolder );

            extractedLib = new File( tempFolder.toFile(), nativeLibraryName );

            String fileToExtract = "/" + osName + "/" + arch + "/" + nativeLibraryName;
            logger.log( Level.FINER, "About to extract {0} from JAR", fileToExtract );
            //now let's extract the proper library
            library = SerialPort.class.getResourceAsStream( fileToExtract );
            Files.copy( library, extractedLib.toPath() );
            extractedLib.deleteOnExit();

            System.load( extractedLib.getAbsolutePath() );
        } catch( IOException e ){
            throw new UnsatisfiedLinkError( "Unable to create temp directory or extract: " + e.getMessage() );
        }
    }

    private class SerialStateListener implements Runnable{

        private volatile boolean stop;
        private SerialChangeListener listen;

        SerialStateListener(SerialChangeListener listen){
            stop = false;
            this.listen = listen;
        }

        @Override
        public void run(){
            while( !stop ){
                synchronized( serialListenSync ){
                    try{
                        serialListenSync.wait();
                        if( stop ){
                            break;
                        }
                        listen.serialStateChanged( state );
                    } catch( Exception e ){
                    }
                }
            }
        }

        void doStop(){
            stop = true;
        }
    }

    /**
     * Represents the BaudRate that the SerialPort uses.
     */
    public enum BaudRate{
        /**
         * Not available in Windows
         */
        B0(0),
        /**
         * Not available in Windows
         */
        B50(50),
        /**
         * Not available in Windows
         */
        B75(75),
        B110(110),
        /**
         * Not available in Windows
         */
        B134(134),
        /**
         * Not available in Windows
         */
        B150(150),
        B200(200),
        B300(300),
        B600(600),
        B1200(1200),
        /**
         * Not available in Windows
         */
        B1800(1800),
        B2400(2400),
        B4800(4800),
        B9600(9600),
        B19200(19200),
        B38400(38400),
        B115200(115200)
        ;

        private final int m_baudRate;

        BaudRate( int baud ){
            m_baudRate = baud;
        }

        public int getBaudRate(){
            return m_baudRate;
        }
    }

    /**
     * Represents the number of bits that the serial port uses. Typically, this
     * is 8-bit characters.
     */
    public enum DataBits{
        DATABITS_5(5),
        DATABITS_6(6),
        DATABITS_7(7),
        DATABITS_8(8)
        ;

        private final int m_dataBits;

        DataBits( int dataBits ){
            m_dataBits = dataBits;
        }

        public int getDataBits(){
            return m_dataBits;
        }
    }

    /**
     * The number of stop bits for data. Typically this is 1.
     */
    public enum StopBits{
        STOPBITS_1(1),
        STOPBITS_2(2)
        ;

        private final int m_stopBits;

        StopBits( int stopBits ){
            m_stopBits = stopBits;
        }

        public int getStopBits(){
            return m_stopBits;
        }
    }

    /**
     * The parity bit for the data. Typically None.
     */
    public enum Parity{
        NONE(0),
        ODD(1),
        EVEN(2)
        ;

        private final int m_parity;

        Parity( int parity ){
            m_parity = parity;
        }

        public int getParity(){
            return m_parity;
        }
    }

    /**
     * The Flow control scheme for the data, typically None.
     */
    public enum FlowControl{
        NONE(0),
        HARDWARE(1),
        SOFTWARE(2)
        ;

        private final int m_flowControl;

        FlowControl( int flowControl ){
            m_flowControl = flowControl;
        }

        public int getFlowControl(){
            return m_flowControl;
        }
    }

    /**
     * Flag to set if you do not want to get any control line notifications
     */
    public static final int NO_CONTROL_LINE_CHANGE = 0x00;
    /**
     * Flag to set if you want to get notifications on Data Terminal Ready line
     * change
     */
    public static final int CONTROL_LINE_DTR_CHANGE = 0x01;
    /**
     * Flag to set if you want to get notifications on Request To Send line
     * change
     */
    public static final int CONTROL_LINE_RTS_CHANGE = 0x02;
    /**
     * Flag to set if you want to get notifications on Carrier Detect line
     * change
     */
    public static final int CONTROL_LINE_CD_CHANGE = 0x03;
    /**
     * Flag to set if you want to get notifications on Clear To Send line change
     */
    public static final int CONTROL_LINE_CTS_CHANGE = 0x04;
    /**
     * Flag to set if you want to get notifications on Data Set Ready line
     * change
     */
    public static final int CONTROL_LINE_DSR_CHANGE = 0x05;
    /**
     * Flag to set if you want to get notifications on Ring Indicagor line
     * change
     */
    public static final int CONTROL_LINE_RI_CHANGE = 0x06;
    /**
     * All CONTROL_LINE_XXX_CHANGE values OR'd together
     */
    public static final int ALL_CONTROL_LINES = CONTROL_LINE_DTR_CHANGE
            | CONTROL_LINE_RTS_CHANGE | CONTROL_LINE_CD_CHANGE | CONTROL_LINE_CTS_CHANGE
            | CONTROL_LINE_DSR_CHANGE | CONTROL_LINE_RI_CHANGE;

    /* The handle to our internal data structure which keeps track of the port settings.
	 * We need a special structure, as on windows we have a HANDLE type, which is void*,
	 * yet on Linux we have a file descriptor, which is an int.
	 * This is not just a pointer to memory, because if we're running on a 64-bit
	 * system, then we might have problems putting it in 32-bits.  Better safe than sorry.
     */
    private int handle;
    /* Make sure we don't close ourselves twice */
    private boolean closed;
    /* The name of the port that's currently open */
    private String portName;
    /* Cache of the last gotten serial line state */
    private volatile SerialLineState state;
    /* The input stream that user code uses to read from the serial port. */
    private SimpleSerialInputStream simpleSerialInputStream;
    /* The buffered serial input stream which filters out events for us. */
    private BufferedSerialInputStream bis;
    /* The output stream that user code uses to write to the serial port. */
    private SerialOutputStream outputStream;
    /* Runs in the background to check for serial port events */
    private SerialStateListener serialListen;
    /* Used for synchronizing serialListen */
    private Object serialListenSync;
    /* Depending on what control line changes we want to get back, this mask is set. */
    private int controlLineFlags;
    /* Flag to determine if we want an InputStream.read() to throw an IOException when interrupted */
    private boolean throwIOExceptionOnInterrupt;

    /**
     * Open the specified port, using parameters from the given builder.
     *
     * @param builder The builder to construct the SerialPort from.
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort( SerialPortBuilder builder )
            throws NoSuchPortException, NotASerialPortException, IOException {
        doOpenSerialPort( builder.portName,
                builder.baudRate,
                builder.dataBits,
                builder.stopBits,
                builder.parity,
                builder.flowControl,
                builder.controlFlags );
    }

    /**
     * Open the specified port, 9600 baud, 8 data bits, 1 stop bit, no parity,
     * no flow control, not ignoring control lines
     *
     * @param portName The name of the port to open
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, 9600 baud, 8 data bits, 1 stop bit, no parity,
     * no flow control, looking for the specified control lines
     *
     * @param portName The name of the port to open
     * @param controlLineFlags The control lines to get a notification for when
     * they change.
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, int controlLineFlags)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, BaudRate.B9600, controlLineFlags );
    }

    /**
     * Open up a serial port, but allow the user to keep the current settings of
     * the serial port. Will notify on all control line changes.
     *
     * @param portName The port to open
     * @param keepSettings If true, will simply open the serial port without
     * setting anything. If false, this method acts the same as {@link #SerialPort(String) SerialPort( String portName )
     * }
     * @throws NoSuchPortException If the port does not exist
     * @throws NotASerialPortException If the port is not in fact a serial port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, boolean keepSettings)
            throws NoSuchPortException, NotASerialPortException, IOException {
        this( portName, keepSettings, ALL_CONTROL_LINES );
    }

    /**
     * Open up a serial port, but allow the user to keep the current settings of
     * the serial port.
     *
     * @param portName The port to open
     * @param keepSettings If true, will simply open the serial port without
     * setting anything. If false, this method acts the same as {@link #SerialPort(String) SerialPort( String portName )
     * }
     * @param controlFlags The control flags to listen for changes for. This is
     * a bitwise-OR of CONTROL_LINE_XXX_CHANGE, or NO_CONTROL_LINE_CHANGE if you
     * don't care about getting notified about the control line changes.
     * @throws NoSuchPortException If the port does not exist
     * @throws NotASerialPortException If the port is not in fact a serial port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, boolean keepSettings, int controlFlags)
            throws NoSuchPortException, NotASerialPortException, IOException{
        if( portName == null ){
            throw new IllegalArgumentException( "portName must not be null" );
        }

        if( keepSettings ){
            this.handle = -1;
            this.handle = openPort( portName );
            this.portName = portName;
            if( controlLineFlags == NO_CONTROL_LINE_CHANGE ){
                logger.log( Level.FINE, "Creating a new SimpleSerialInputStream - not monitoring for control line change" );
                simpleSerialInputStream = new SimpleSerialInputStream( handle );
            } else{
                logger.log( Level.FINE, "Creating a new BufferedSerialInputStream - monitoring for control line change" );
                SerialInputStream sis = new SerialInputStream( handle );
                bis = new BufferedSerialInputStream( sis, this );
            }
            outputStream = new SerialOutputStream( handle );
            closed = false;
            this.controlLineFlags = controlFlags;

            SerialLineState s = new SerialLineState();
            int state = getSerialLineStateInternalNonblocking();
            // do some sort of bitwise operations here....
            if( (state & 0x01) > 0 ){
                s.carrierDetect = true;
            }
            if( (state & (0x01 << 1)) > 0 ){
                s.clearToSend = true;
            }
            if( (state & (0x01 << 2)) > 0 ){
                s.dataSetReady = true;
            }
            if( (state & (0x01 << 3)) > 0 ){
                s.dataTerminalReady = true;
            }
            if( (state & (0x01 << 4)) > 0 ){
                s.requestToSend = true;
            }
            if( (state & (0x01 << 5)) > 0 ){
                s.ringIndicator = true;
            }

            this.state = s;

            if( controlLineFlags != NO_CONTROL_LINE_CHANGE ){
                serialListenSync = new Object();
                new Thread( bis, "BufferedSerialReader-" + portName ).start();
            }
        } else{
            doOpenSerialPort( portName, BaudRate.B9600, DataBits.DATABITS_8,
                    StopBits.STOPBITS_1, Parity.NONE, FlowControl.NONE, controlFlags );
        }

    }

    /**
     * Open the specified port, no flow control, 8 data bits
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, rate, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, no flow control
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param controlLines The control lines to be notifie don a change in.
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, int controlLines)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, rate, DataBits.DATABITS_8, controlLines );
    }

    /**
     * Open the specified port, no flow control
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, rate, data, StopBits.STOPBITS_1, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, no flow control
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, int controlLineFlags)
            throws NoSuchPortException, NotASerialPortException, IOException {
        doOpenSerialPort( portName, rate, data,
                StopBits.STOPBITS_1, Parity.NONE, FlowControl.NONE, controlLineFlags );
    }

    /**
     * Open the specified port, no parity or flow control
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop)
            throws NoSuchPortException, NotASerialPortException, IOException {
        doOpenSerialPort( portName, rate, data,
                stop, Parity.NONE, FlowControl.NONE, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, no parity or flow control
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, int controlFlags)
            throws NoSuchPortException, NotASerialPortException, IOException{
        doOpenSerialPort( portName, rate, data,
                stop, Parity.NONE, FlowControl.NONE, controlFlags );
    }

    /**
     * Open the specified port, no flow control, with all control line flags
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @param parity The parity of the line
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, Parity parity)
            throws NoSuchPortException, NotASerialPortException, IOException{
        doOpenSerialPort( portName, rate, data, stop, parity, FlowControl.NONE, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, no flow control, with the specified control line
     * flags
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @param parity The parity of the line
     * @param controlLineFlags A bitwise OR of any the CONTORL_LINE_XXX_CHANGE
     * flags, or NO_CONTROL_LINE_CHANGE
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, Parity parity, int controlLineFlags)
            throws NoSuchPortException, NotASerialPortException, IOException{
        this( portName, rate, data, stop, parity, FlowControl.NONE, controlLineFlags );
    }

    /**
     * Open the specified port, defining all options
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @param parity The parity of the line
     * @param flow The flow control of the line
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, Parity parity, FlowControl flow)
            throws NoSuchPortException, NotASerialPortException, IOException {
        this( portName, rate, data, stop, parity, flow, ALL_CONTROL_LINES );
    }

    /**
     * Open the specified port, defining all options
     *
     * @param portName The name of the port to open
     * @param rate The Baud Rate to open this port at
     * @param data The number of data bits
     * @param stop The number of stop bits
     * @param parity The parity of the line
     * @param flow The flow control of the line
     * @param controlFlags The control flags to listen for changes for. This is
     * a bitwise-OR of CONTROL_LINE_XXX_CHANGE, or NO_CONTROL_LINE_CHANGE if you
     * don't care about getting notified about the control line changes.
     * @throws NoSuchPortException If this port does not exist
     * @throws NotASerialPortException If the specified port is not a serial
     * port
     * @throws IOException if a low-level error occurs
     */
    public SerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, Parity parity, FlowControl flow, int controlFlags)
            throws NoSuchPortException, NotASerialPortException, IOException {
        doOpenSerialPort( portName, rate, data, stop, parity, flow, controlFlags );
    }

    /**
     * The actual method that opens up the serial port, unless we are keeping
     * our settings.
     */
    private void doOpenSerialPort(String portName, BaudRate rate, DataBits data, StopBits stop, Parity parity, FlowControl flow, int controlFlags)
            throws NoSuchPortException, NotASerialPortException, IOException{
        SerialLineState s;
        int state;
        SerialInputStream sis;

        this.handle = -1;
        throwIOExceptionOnInterrupt = false;

        //Check for null values in our arguments
        if( portName == null ){
            throw new IllegalArgumentException( "portName must not be null" );
        }

        if( rate == null ){
            throw new IllegalArgumentException( "rate must not be null" );
        }

        if( data == null ){
            throw new IllegalArgumentException( "data must not be null" );
        }

        if( stop == null ){
            throw new IllegalArgumentException( "stop must not be null" );
        }

        if( parity == null ){
            throw new IllegalArgumentException( "parity must not be null" );
        }

        if( flow == null ){
            throw new IllegalArgumentException( "flow must not be null" );
        }

        logger.log( Level.INFO, "Opening up serial port {0} with the following settings: "
                + "speed:{1} data:{2} stop:{3} parity:{4} flow:{5}",
                new Object[] {
                    portName,
                    rate,
                    data,
                    stop,
                    parity,
                    flow
                });

        //Okay, looks like we're good!
        this.portName = portName;
        closed = false;
        this.controlLineFlags = controlFlags;

        handle = openPort( portName,
                rate.getBaudRate(),
                data.getDataBits(),
                stop.getStopBits(),
                parity.getParity(),
                flow.getFlowControl() );
        if( controlLineFlags == NO_CONTROL_LINE_CHANGE ){
            logger.log( Level.FINE, "Creating a new SimpleSerialInputStream - not monitoring for control line change" );
            simpleSerialInputStream = new SimpleSerialInputStream( handle );
        } else{
            logger.log( Level.FINE, "Creating a new BufferedSerialInputStream - monitoring for control line change" );
            sis = new SerialInputStream( handle );
            bis = new BufferedSerialInputStream( sis, this );
        }
        outputStream = new SerialOutputStream( handle );

        s = new SerialLineState();
        state = getSerialLineStateInternalNonblocking();
        if( (state & 0x01) > 0 ){
            s.carrierDetect = true;
        }
        if( (state & (0x01 << 1)) > 0 ){
            s.clearToSend = true;
        }
        if( (state & (0x01 << 2)) > 0 ){
            s.dataSetReady = true;
        }
        if( (state & (0x01 << 3)) > 0 ){
            s.dataTerminalReady = true;
        }
        if( (state & (0x01 << 4)) > 0 ){
            s.requestToSend = true;
        }
        if( (state & (0x01 << 5)) > 0 ){
            s.ringIndicator = true;
        }

        this.state = s;

        if( controlLineFlags != NO_CONTROL_LINE_CHANGE ){
            serialListenSync = new Object();
            new Thread( bis, "BufferedSerialReader-" + portName ).start();
        }
    }

    /**
     * Set the Baud Rate for this port.
     *
     * @param rate
     */
    public void setBaudRate(BaudRate rate) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set the BaudRate once the port has been closed." );
        }

        if( rate == null ){
            throw new IllegalArgumentException( "rate must not be null" );
        }

        setBaudRate( rate.getBaudRate() );
    }

    public boolean isClosed(){
        return closed;
    }

    public void finalize(){
        close();
    }

    public void close(){
        if( closed ){
            return;
        }
        closed = true;
        doClose();
        if( serialListen != null ){
            serialListen.doStop();
        }

        if( serialListenSync != null ){
            synchronized( serialListenSync ){
                serialListenSync.notify();
            }
        }
    }

    /**
     * Get the input stream used to talk to this device.
     */
    public InputStream getInputStream(){
        if( isClosed() ){
            throw new IllegalStateException( "Cannot get the input stream once the port has been closed." );
        }

        if( bis != null ){
            return bis;
        }

        if( simpleSerialInputStream != null ){
            return simpleSerialInputStream;
        }

        logger.log( Level.SEVERE, "Tried to get input stream, but both BufferedSerialInputStream and SimpleSerialInputStream null" );

        return null;
    }

    /**
     * Set if using Thread.interrupt() will throw an IO exception.
     *
     * <b>Note:</b> This is OS and implementation-specific.  Depending on how the serial port
     * is opened, this value may have no effect.
     *
     * This will work in the following situations:
     * <ul>
     * <li>Opening a serial port, monitoring the serial lines(e.g. NO_SERIAL_LINE_CHANGE was not passed to the constructor)</li>
     * </ul>
     *
     *
     * @param interruptCausesIOException
     */
    public void setInterruptCausesIOException( boolean interruptCausesIOException ){
        this.throwIOExceptionOnInterrupt = interruptCausesIOException;
        if( bis != null ){
            bis.setInterruptCausesIOException( interruptCausesIOException );
        }else if( simpleSerialInputStream != null ){

        }

    }

    /**
     * Get the OutputStream used to talk to this device.
     */
    public OutputStream getOutputStream(){
        if( isClosed() ){
            throw new IllegalStateException( "Cannot get the output stream once the port has been closed." );
        }

        return outputStream;
    }

    /**
     * Set the stop bits of the serial port, after the port has been opened.
     *
     * @param stop
     */
    public void setStopBits(StopBits stop) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set the StopBits once the port has been closed." );
        }

        if( stop == null ){
            throw new IllegalArgumentException( "stop must not be null" );
        }

        setStopBits( stop.getStopBits() );
    }

    /**
     * Set the data bits size, after the port has been opened.
     *
     * @param data
     */
    public void setDataSize(DataBits data) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set the DataBits once the port has been closed." );
        }

        if( data == null ){
            throw new IllegalArgumentException( "data must not be null" );
        }

        setCharSize( data.getDataBits() );
    }

    /**
     * Set the parity of the serial port, after the port has been opened.
     *
     * @param parity
     */
    public void setParity(Parity parity) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set the parity once the port has been closed." );
        }

        if( parity == null ){
            throw new IllegalArgumentException( "parity must not be null" );
        }

        setParity( parity.getParity() );
    }

    /**
     * Get the serial line state for the specified serial port.
     *
     * @return
     */
    public SerialLineState getSerialLineState() throws IOException{
        if( closed ){
            throw new IllegalStateException( "Cannot get the serial line state once the port has been closed." );
        }

        int gotState = getSerialLineStateInternalNonblocking();

        SerialLineState s = new SerialLineState();
        // do some sort of bitwise operations here....
        if( (gotState & 0x01) > 0 ){
            s.carrierDetect = true;
        }
        if( (gotState & (0x01 << 1)) > 0 ){
            s.clearToSend = true;
        }
        if( (gotState & (0x01 << 2)) > 0 ){
            s.dataSetReady = true;
        }
        if( (gotState & (0x01 << 3)) > 0 ){
            s.dataTerminalReady = true;
        }
        if( (gotState & (0x01 << 4)) > 0 ){
            s.requestToSend = true;
        }
        if( (gotState & (0x01 << 5)) > 0 ){
            s.ringIndicator = true;
        }

        return s;
    }

    /**
     * Set the serial line state to the parameters given.
     *
     * @param state
     */
    public void setSerialLineState(SerialLineState state) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set the serial line state once the port has been closed." );
        }

        setSerialLineStateInternal( state );

        //Now, because Windows is weird, we need to post a serial changed event here.
        //however, since we can only set DTR and RTS, only post an event if those are
        //the things that changed
//		if( this.state.dataTerminalReady != state.dataTerminalReady ||
//				this.state.requestToSend != state.requestToSend ){
//			this.postSerialChangedEvent( state );
//		}
    }

    /**
     * Get the baud rate of the serial port.
     *
     * @return
     * @throws IOException if a low-level error occurs
     */
    public BaudRate getBaudRate() throws IOException{
        int baudRate;

        if( closed ){
            throw new IllegalStateException( "Cannot get the baud rate once the port has been closed." );
        }

        baudRate = getBaudRateInternal();
        for( BaudRate b : BaudRate.values() ){
            if( b.getBaudRate() == baudRate ){
                return b;
            }
        }

        return BaudRate.B0;
    }

    /**
     * Get the number of data bits.
     *
     * @return
     * @throws IOException if a low-level error occurs
     */
    public DataBits getDataBits() throws IOException{
        int dataBits;

        if( closed ){
            throw new IllegalStateException( "Cannot get the data bits once the port has been closed." );
        }

        dataBits = getCharSizeInternal();

        for( DataBits d : DataBits.values() ){
            if( dataBits == d.getDataBits() ){
                return d;
            }
        }

        return DataBits.DATABITS_8;
    }

    /**
     * Get the number of stop bits.
     *
     * @return
     * @throws IOException if a low-level error occurs
     */
    public StopBits getStopBits() throws IOException {
        int stopBits;

        if( closed ){
            throw new IllegalStateException( "Cannot get stop bits once the port has been closed." );
        }

        stopBits = getStopBitsInternal();
        for( StopBits sb : StopBits.values() ){
            if( sb.getStopBits() == stopBits ){
                return sb;
            }
        }


        return StopBits.STOPBITS_1;
    }

    /**
     * Get the parity of the serial port.
     *
     * @return
     * @throws IOException if a low-level error occurs
     */
    public Parity getParity() throws IOException {
        int parity;

        if( closed ){
            throw new IllegalStateException( "Cannot get the parity once the port has been closed." );
        }

        parity = getParityInternal();
        for( Parity par : Parity.values() ){
            if( par.getParity() == parity ){
                return par;
            }
        }

        return Parity.NONE;
    }

    /**
     * Get the flow control for the serial port.
     *
     * @return
     * @throws IOException if a low-level error occurs
     */
    public FlowControl getFlowControl() throws IOException {
        int flowControl;

        if( closed ){
            throw new IllegalStateException( "Cannot get the flow once the port has been closed." );
        }

        flowControl = getFlowControlInternal();
        for( FlowControl control : FlowControl.values() ){
            if( control.getFlowControl() == flowControl ){
                return control;
            }
        }

        return FlowControl.NONE;
    }

    /**
     * Set the flow control for the serial port
     *
     * @param flow
     * @throws IOException if a low-level error occurs
     */
    public void setFlowControl(FlowControl flow) throws IOException {
        if( closed ){
            throw new IllegalStateException( "Cannot set flow once the port has been closed." );
        }

        setFlowControl( flow.getFlowControl() );
    }

    /**
     * Set the listener which will get events when there is activity on the
     * serial port. Note that this activity does NOT include receive and
     * transmit events - this is changes on the lines of the serial port, such
     * as RI, DSR, and DTR.
     *
     * If listen is null, will remove the listener.
     *
     * @param listen The listener which gets events
     */
    public void setSerialChangeListener(SerialChangeListener listen){
        if( serialListen != null ){
            serialListen.doStop();
        }

        if( listen != null && !(controlLineFlags == NO_CONTROL_LINE_CHANGE) ){
            serialListen = new SerialStateListener( listen );
            new Thread( serialListen, "SerialListen-" + portName ).start();
        }

    }

    /**
     * Get the name of the serial port that this object represents.
     *
     * @return
     */
    public String getPortName(){
        return portName;
    }

    /**
     * This method is called when the state of the serial lines is changed.
     *
     * @param newState
     */
    void postSerialChangedEvent(SerialLineState newState){
        if( !state.equals( newState ) ){
            SerialLineState oldState = state;
            boolean update = false;
            state = newState;

            //At this point, we know what has changed, but we must check our bitmask to see if we should
            //propogate this change back up to the interested class.
            if( oldState.carrierDetect != newState.carrierDetect
                    && (controlLineFlags & CONTROL_LINE_CD_CHANGE) != 0 ){
                update = true;
            }
            if( oldState.clearToSend != newState.clearToSend
                    && (controlLineFlags & CONTROL_LINE_CTS_CHANGE) != 0 ){
                update = true;
            }
            if( oldState.dataSetReady != newState.dataSetReady
                    && (controlLineFlags & CONTROL_LINE_DSR_CHANGE) != 0 ){
                update = true;
            }
            if( oldState.dataTerminalReady != newState.dataTerminalReady
                    && (controlLineFlags & CONTROL_LINE_DTR_CHANGE) != 0 ){
                update = true;
            }
            if( oldState.requestToSend != newState.requestToSend
                    && (controlLineFlags & CONTROL_LINE_RTS_CHANGE) != 0 ){
                update = true;
            }
            if( oldState.ringIndicator != newState.ringIndicator
                    && (controlLineFlags & CONTROL_LINE_RI_CHANGE) != 0 ){
                update = true;
            }

            if( update ){
                synchronized( serialListenSync ){
                    serialListenSync.notify();
                }
            }
        }
    }

    /**
     * Open the specified port, return an internal handle to the data structure
     * for this port.
     *
     * @param portName
     * @return
     */
    private native int openPort(String portName, int baudRate, int dataBits, int stopBits, int parity, int flowControl)
            throws NoSuchPortException, NotASerialPortException, IOException;

    /**
     * Open the specified port, return an internal handle for the data of this
     * port. This method DOES NOT set any of the serial port settings
     *
     * @param portName The port to open
     * @return
     */
    private native int openPort(String portName)
            throws NoSuchPortException, NotASerialPortException, IOException;

    /**
     * Close this port, release all native resources
     */
    private native void doClose();

    /**
     * Set the baud rate in the native code.
     *
     * @param baudRate
     * @return
     */
    private native boolean setBaudRate(int baudRate) throws IOException;

    private native int getBaudRateInternal() throws IOException;

    /**
     * Set the number of stop bits, once the port has been opened.
     *
     * @param stopBits
     * @return
     */
    private native boolean setStopBits(int stopBits) throws IOException;

    private native int getStopBitsInternal() throws IOException;

    /**
     * Set the character size, once the port has been opened. This should
     * probably be called 'setDataBits'
     *
     * @param charSize
     * @return
     */
    private native boolean setCharSize(int charSize) throws IOException;

    private native int getCharSizeInternal() throws IOException;

    /**
     * Set the parity once the port has been opened.
     *
     * @param parity 0 = None, 1 = Odd, 2 = Even
     * @return
     */
    private native boolean setParity(int parity) throws IOException;

    private native int getParityInternal() throws IOException;

    /**
     * Set the flow control once the port has been opened.
     *
     *
     * @param flowControl 0 = None, 1 = hardware, 2 = software
     * @return
     */
    private native boolean setFlowControl(int flowControl) throws IOException;

    private native int getFlowControlInternal() throws IOException;

    /**
     * Get the serial line state, but don't block when getting it
     *
     * @return
     */
    protected native int getSerialLineStateInternalNonblocking() throws IOException;

    /**
     * Set the state of the serial line.
     *
     * @return
     */
    private native int setSerialLineStateInternal(SerialLineState s) throws IOException;

    //
    // Static Methods
    //
    /**
     * Get the major version of this library. For example, if this is version
     * 0.2, this returns 0
     */
    public static int getMajorVersion(){
        return 0;
    }

    /**
     * Get the minor version of this library. For example, if this is version
     * 0.2, this returns 2.
     */
    public static int getMinorVersion(){
        return 9;
    }

    /**
     * Get the major version of the native code. This should match up with
     * {@link #getMajorVersion() getMajorVersion()}, although this is not
     * guaranteed. For example, if this is version 0.2, this returns 0
     */
    public static native int getMajorNativeVersion();

    /**
     * Get the minor version of the native code. This should match up with
     * {@link #getMinorVersion() getMinorVersion()}, although this is not
     * guaranteed. For example, if this is version 0.2, this returns 2.
     */
    public static native int getMinorNativeVersion();

    /**
     * <p>
     * Get an array of all the serial ports on the system. For example, on
     * Windows this will return {@code { "COM1", "COM3", .... } } depending on
     * how many serial devices you have plugged in. On Linux, this returns
     * {@code { "/dev/ttyS0", "/dev/ttyUSB0", "/dev/symlink", ... } }
     * It will not resolve symlinks, such that if there is a symlink from {@code /dev/symlink
     * } to {@code /dev/ttyUSB0 }, they will both show up.
     * </p>
     * <p>
     * <b>NOTE:</b> this will only return ports that you have permissions to
     * open.
     * </p>
     *
     * @return
     */
    public static native String[] getSerialPorts() throws IOException;

}
