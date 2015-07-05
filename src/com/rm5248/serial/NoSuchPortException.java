package com.rm5248.serial;

/**
 * This exception is thrown when you attempt to open up a port that does not exist.
 *  
 * @author rm5248
 *
 */
public class NoSuchPortException extends Exception {

	/**
	 * Generated by Eclipse
	 */
	private static final long serialVersionUID = -5218448691940726352L;

	/**
	 * Create a new exception with the specified error.
	 * 
	 * @param status
	 */
	public NoSuchPortException( String status ){
		super( status );
	}
}
