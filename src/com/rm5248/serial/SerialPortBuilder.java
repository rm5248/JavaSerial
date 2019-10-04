package com.rm5248.serial;

import java.io.IOException;

/**
 * Provides a Builder pattern to make constructing a SerialPort easy.
 */
public class SerialPortBuilder {

    String portName;
    SerialPort.BaudRate baudRate;
    SerialPort.DataBits dataBits;
    SerialPort.FlowControl flowControl;
    SerialPort.Parity parity;
    SerialPort.StopBits stopBits;
    int controlFlags;

    /**
     * Create a new SerialPortBuilder.  Defaults to the same settings as
     * SerialPort(String), e.g. 9600-8-N-1
     */
    public SerialPortBuilder(){
        baudRate = SerialPort.BaudRate.B9600;
        dataBits = SerialPort.DataBits.DATABITS_8;
        flowControl = SerialPort.FlowControl.NONE;
        parity = SerialPort.Parity.NONE;
        stopBits = SerialPort.StopBits.STOPBITS_1;
        controlFlags = SerialPort.ALL_CONTROL_LINES;
    }

    SerialPortBuilder setPort( String portName ){
        this.portName = portName;
        return this;
    }

    SerialPortBuilder setBaudRate( SerialPort.BaudRate baudRate ){
        this.baudRate = baudRate;
        return this;
    }

    SerialPortBuilder setDataBits( SerialPort.DataBits dataBits ){
        this.dataBits = dataBits;
        return this;
    }

    SerialPortBuilder setFlowControl( SerialPort.FlowControl flowControl ){
        this.flowControl = flowControl;
        return this;
    }

    SerialPortBuilder setParity( SerialPort.Parity parity ){
        this.parity = parity;
        return this;
    }

    SerialPortBuilder setStopBits( SerialPort.StopBits stopBits ){
        this.stopBits = stopBits;
        return this;
    }

    SerialPortBuilder setControlFlags( int controlFlags ){
        this.controlFlags = controlFlags;
        return this;
    }

    SerialPort build() throws NoSuchPortException, NotASerialPortException, IOException {
        return new SerialPort( this );
    }
}
