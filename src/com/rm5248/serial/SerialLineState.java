package com.rm5248.serial;

/**
 * This class represents the current state of the lines on the serial port.
 * You can also use this class to set the state of the serial port.
 * 
 * Note that all of these options may not be available depending on the 
 * serial port that is in use, and/or the platform that you are running on.
 *
 */
public class SerialLineState {
	
	public boolean carrierDetect;
	public boolean clearToSend;
	public boolean dataSetReady;
	public boolean dataTerminalReady;
	public boolean ringIndicator;
	public boolean requestToSend;
	
	public SerialLineState(){
		carrierDetect = false;
		clearToSend = false;
		dataSetReady = false;
		dataTerminalReady = false;
		ringIndicator = false;
		requestToSend = false;
	}
	
	@Override
	public boolean equals( Object o ){
		if( o instanceof SerialLineState ){
			SerialLineState s = (SerialLineState)o;
			
			return ( s.carrierDetect == carrierDetect ) && 
					( s.clearToSend == clearToSend ) &&
					( s.dataSetReady == dataSetReady ) &&
					( s.dataTerminalReady == dataTerminalReady ) &&
					( s.ringIndicator == ringIndicator ) &&
					( s.requestToSend == requestToSend );
		}
		
		return false;
	}
	
	@Override
	public String toString(){
		return "[SerialLineState: " + 
				"CD: " + carrierDetect + 
				" CTS: " + clearToSend +
				" DSR: " + dataSetReady +
				" DTR: " + dataTerminalReady +
				" RI: " + ringIndicator +
				" RTS: " + requestToSend + "]";
	}

}
