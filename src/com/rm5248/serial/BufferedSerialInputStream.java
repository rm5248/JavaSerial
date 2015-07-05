package com.rm5248.serial;

import java.io.IOException;
import java.io.InputStream;

/**
 * Okay, so the problem here is that we need to be continually reading from the serial port,
 * because on Windows we can't simultaneously listen for events and listen for changes on
 * the serial port.  We can only do that in one place.  So, to get around that problem,
 * this BufferedSerialInputStream simply wraps the normal SerialInputStream and only 
 * returns actual data.  SerialInputStream gives us back data on the status of the serial line,
 * as well as if the byte it gave us is real or not.
 * 
 * @author rm5248
 *
 */
class BufferedSerialInputStream extends InputStream implements Runnable {

	private SerialInputStream stream;
	private byte[] buffer;
	private int bufferBegin;
	private int bufferEnd;
	private SerialPort callback;
	private IOException exceptionToThrow;

	BufferedSerialInputStream( SerialInputStream s, SerialPort serialPort ){
		stream = s;
		buffer = new byte[ 500 ];
		bufferBegin = 0;
		bufferEnd = 0;
		this.callback = serialPort;
		exceptionToThrow = null;
	}

	@Override
	public int read() throws IOException {
		int byteToReturn;
		
		if( bufferBegin != bufferEnd ){
			//we already have a character 
			byteToReturn = buffer[ bufferBegin++ ];
			if( bufferBegin >= buffer.length ){
				//wrap around to the start of the array
				bufferBegin = 0;
			}
		}else{
			synchronized( buffer ){
				try {
					buffer.wait();
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
				
				if( exceptionToThrow != null ){
					throw exceptionToThrow;
				}
			}
			
			byteToReturn = buffer[ bufferBegin++ ];
			if( bufferBegin >= buffer.length ){
				//wrap around to the start of the array
				bufferBegin = 0;
			}
		}
		
		return byteToReturn;
	}

	@Override
	public void run() {
		while( true ){
			int byteRead = 0;
			try {
				byteRead = stream.read();
			} catch (IOException e) {
				synchronized( buffer ){
					exceptionToThrow = e;
					buffer.notify();
				}
				break;
			}
			SerialLineState s = new SerialLineState();

			if( ( byteRead & ( 0x01 << 9) ) > 0 ){
				s.carrierDetect = true;
			}
			if( ( byteRead & (0x01 << 10 ) ) > 0  ){
				s.clearToSend = true;
			}
			if( ( byteRead & (0x01 << 11 ) ) > 0  ){
				s.dataSetReady = true;
			}
			if( ( byteRead & (0x01 << 12 ) ) > 0  ){
				s.dataTerminalReady = true;
			}
			if( ( byteRead & (0x01 << 13 ) ) > 0  ){
				s.requestToSend = true;
			}
			if( ( byteRead & (0x01 << 14 ) ) > 0  ){
				s.ringIndicator = true;
			}

			if( ( byteRead & ( 0x01 << 15 ) ) > 0 ){
				//this is a valid byte
				synchronized( buffer ){
					if( bufferEnd + 1 >= buffer.length ){
						//loop back around to the beginning
						bufferEnd = 0;
					}
					buffer[ bufferEnd++ ] = (byte)( byteRead & 0xFF );
					if( bufferEnd == bufferBegin ){
						//the end has wrapped around, increment the beginning
						bufferBegin++;
												
						if( bufferBegin >= buffer.length ){
							bufferBegin = 0;
						}
					}
					
					buffer.notify();
				}
			}
			
			callback.postSerialChangedEvent( s );
		}
	}
	
	@Override
	public int available(){
		if( bufferEnd < bufferBegin ){
			//special case - we have looped around
			return ( buffer.length - bufferBegin ) + bufferEnd;
		}else{
			return bufferEnd - bufferBegin;
		}
	}
	
	@Override
	public int read( byte[] b, int off, int len ) throws IOException{
		int readSoFar = 0;
		
		if( len == 0 ){
			return 0;
		}
		
		for( int x = off; x < len; x++ ){
			b[x] = (byte)read();
			readSoFar++;
			if( readSoFar > 0 && available() == 0 ){
				break;
			}
		}
		
		return readSoFar;
	}

}
