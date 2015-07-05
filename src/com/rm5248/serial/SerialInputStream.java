package com.rm5248.serial;

import java.io.IOException;
import java.io.InputStream;

/**
 * Input stream for the serial port.  This implementation passes back the 
 * status of the control lines in the upper bits of {@code read()}, and 
 * thus must be parsed properly.
 */
class SerialInputStream extends InputStream{
	/* The handle to read from.  Needed for native implementation */
	@SuppressWarnings("unused")
	private int handle;
	

	SerialInputStream( int handle ){
		this.handle = handle;
	}

	@Override
	public int read() throws IOException{
		return readByte();
	}

	@Override
	public int available() throws IOException{
		return getAvailable();
	}

	private native int readByte() throws IOException;

	private native int getAvailable() throws IOException;
}

/**
 * A SimpleSerialInputStream does not pass back the status of the control
 * lines on the serial port - just the raw bytes.
 */
class SimpleSerialInputStream extends InputStream {
	/* The handle to read from.  Needed for native implementation */
	@SuppressWarnings("unused")
	private int handle;
	

	SimpleSerialInputStream( int handle ){
		this.handle = handle;
	}

	@Override
	public int read() throws IOException{
		return readByte();
	}

	@Override
	public int available() throws IOException{
		return getAvailable();
	}

	private native int readByte() throws IOException;

	private native int getAvailable() throws IOException;
}