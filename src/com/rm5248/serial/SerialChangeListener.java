package com.rm5248.serial;

/**
 * This interface lets the user do something when the serial line state changes.
 * The state is defined by how the lines are currently set.
 *
 */
public interface SerialChangeListener {

	/**
	 * Fired when the state on the serial line changes.
	 * 
	 * @param state
	 */
	public void serialStateChanged( SerialLineState state );
}
