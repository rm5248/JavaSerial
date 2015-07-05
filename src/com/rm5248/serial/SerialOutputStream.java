package com.rm5248.serial;

import java.io.IOException;
import java.io.OutputStream;

class SerialOutputStream extends OutputStream{
	/* The handle to write to */
	@SuppressWarnings("unused")
	private int handle;

	SerialOutputStream( int handle ){
		this.handle = handle;
	}

	@Override
	public void write(int b) throws IOException {
		writeByte( b );
	}
	
	@Override
	public void write( byte[] arr ) throws IOException{
		writeByteArray( arr );
	}

	private native void writeByte( int toWrite ) throws IOException;
	
	private native void writeByteArray( byte[] array ) throws IOException;
}
