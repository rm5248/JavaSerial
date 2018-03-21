#ifdef _WIN32
	#include <windows.h>
	
	#define SPEED_SWITCH(SPD,io) case SPD: io.BaudRate = CBR_##SPD; break;
	#define GET_SPEED_SWITCH(SPD,io) case CBR_##SPD: return SPD;
	
	#define GET_SERIAL_PORT_STRUCT( port, io_name ) DCB io_name = {0};\
												io_name.DCBlength = sizeof( io_name ); \
												if (!GetCommState( port, &io_name ) ) { \
												printf("bad get comm\n");\
													return -1;\
												}
	#define SET_SERIAL_PORT_STRUCT( port, io_name ) 	if( !SetCommState( port, &io_name ) ){\
	printf("bad set comm\n");\
															return 0;\
														}
														
	#define close(handle) CloseHandle(handle)
#else
	#include <termios.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <dirent.h>
	#include <pthread.h>
	#include <sys/ioctl.h>
	#include <errno.h>
	#include <poll.h>

	#ifndef ENOMEDIUM
	#define ENOMEDIUM ENODEV
	#endif

	#ifdef CRTSCTS
		#define HW_FLOW CRTSCTS
	#elif CNEW_RTSCTS
		#define HW_FLOW CNEW_RTSCTS
	#endif
	
	#define SPEED_SWITCH(SPD,io) case SPD: cfsetospeed( &io, B##SPD ); cfsetispeed( &io, B##SPD ); break;
	#define GET_SPEED_SWITCH(SPD,io) case B##SPD: return SPD;
	
	
	#define GET_SERIAL_PORT_STRUCT( port, io_name )	struct termios io_name; \
													if( tcgetattr( port, &io_name ) < 0 ){ \
														return -1; \
													}
	#define SET_SERIAL_PORT_STRUCT( port, io_name ) 	if( tcsetattr( port, TCSANOW, &io_name ) < 0 ){\
															return -1;\
														}
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>

//
// Local Includes
//
#include "com_rm5248_serial_SerialPort.h"
#include "com_rm5248_serial_SerialInputStream.h"
#include "com_rm5248_serial_SerialOutputStream.h"
#include "com_rm5248_serial_SimpleSerialInputStream.h"

// log levels
#define MESSAGE_DEBUG 0 /* java.util.logging FINE - log4j2 DEBUG */
#define MESSAGE_TRACE 1 /* java.util.logging FINER - log4j2 TRACE */

//
// Struct Definitions
//
struct port_descriptor{
#ifdef _WIN32
	HANDLE port;
	HANDLE in_use;
	//Unfortunately, Windows does not let us get the state of the DTR/RTS lines.
	//So, we need to keep track of that manually.  
	int winDTR;
	int winRTS;
#else
	int port;
	/* There appears to be a case where we can free() the port_descriptor
	 * after the select() call in SerialInputStream_readByte().
	 * This means that we segfault on the FD_SET() call.
	 * This mutex will keep us alive until we have exited the 
	 * readByte() call.
	 *
	 * This also ensures that all current calls into the native code are
	 * finished by the time doClose() returns.
	 */
	pthread_mutex_t in_use;
#endif
};

//
// Local Variables
//
static struct port_descriptor** port_list = NULL;
static int port_list_size;

//
// Helper Methods
//
static void output_to_stderr( char* message ){
	fflush( stdout );	
	fflush( stderr );
	fprintf( stderr, "%s", message );
	fflush( stdout );	
	fflush( stderr );
}

static void log_message( int log_level, JNIEnv * env, char* message ){
	jobject log_obj;
	jclass serial_class;
	jclass logger_class;
	jfieldID logger_id;
	jmethodID log_method_id;
	jstring debug_string;

	logger_class = (*env)->FindClass( env, "java/util/logging/Logger" );
	if( logger_class == NULL ){
		output_to_stderr( "ERROR: Can't find java logger?\n" );
		return;
	}

	serial_class = (*env)->FindClass( env, "com/rm5248/serial/SerialPort" );
	if( serial_class == NULL ){
		output_to_stderr( "ERROR: Can't find serial port class?!\n" );
		return;
	}

	logger_id = (*env)->GetStaticFieldID( env, serial_class, "native_logger", "Ljava/util/logging/Logger;" );
	if( logger_id == NULL ){
		output_to_stderr( "ERROR: Can't find native_logger?!\n" );
		return;
	}
	

	log_obj = (*env)->GetStaticObjectField( env, serial_class, logger_id );
	if( log_obj == NULL ){
		output_to_stderr( "ERROR: Can't do logging?!\n" );
		return;
	}

	if( log_level == MESSAGE_DEBUG ){
		log_method_id = (*env)->GetMethodID( env, logger_class, "fine", "(Ljava/lang/String;)V" );
	}else if( log_level == MESSAGE_TRACE ){
		log_method_id = (*env)->GetMethodID( env, logger_class, "finer", "(Ljava/lang/String;)V" );
	}else{
		output_to_stderr( "PROGRAMMING ERROR: invalid log level provided\n" );
		return;
	}

	if( log_method_id == NULL ){
		output_to_stderr( "ERROR: Can't find 'fine' method on java.util.logging.Logger\n" );
		return;
	}

	debug_string = (*env)->NewStringUTF( env, message );
	if( debug_string == NULL ){
		output_to_stderr( "ERROR: can't constuct string\n" );
		return;
	}

	(*env)->CallVoidMethod( env, log_obj, log_method_id, debug_string );
}

static jint get_handle(JNIEnv * env, jobject obj){
	jfieldID fid;
	jint array_pos;
	jclass cls = (*env)->GetObjectClass( env, obj );

	fid = (*env)->GetFieldID( env, cls, "handle", "I" );
	if( fid == 0 ){
		return -1;
	}

	array_pos = (*env)->GetIntField( env, obj, fid );

	return array_pos;
}

static jboolean get_bool( JNIEnv* env, jobject obj, const char* name ){
	jfieldID fid;
	jboolean boolVal;
	jclass cls = (*env)->GetObjectClass( env, obj );

	fid = (*env)->GetFieldID( env, cls, name, "Z" );
	if( fid == 0 ){
		return 0; //not really sure what error to set here...
	}

	boolVal = (*env)->GetBooleanField( env, obj, fid );

	return boolVal;
}

static int set_baud_rate( struct port_descriptor* desc, int baud_rate ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

	switch( baud_rate ){
#ifndef _WIN32
/* Note that Windows only supports speeds of 110 and above */
		SPEED_SWITCH(0,newio);
		SPEED_SWITCH(50,newio);
		SPEED_SWITCH(75,newio);
#endif
		SPEED_SWITCH(110,newio);
#ifndef _WIN32
/* Windows does not support speeds of 134, 150, or 200 */
		SPEED_SWITCH(134,newio);
		SPEED_SWITCH(150,newio);
		SPEED_SWITCH(200,newio);
#endif
		SPEED_SWITCH(300,newio);
		SPEED_SWITCH(600,newio);
		SPEED_SWITCH(1200,newio);
#ifndef _WIN32
/* Windows does not support 1800 */
		SPEED_SWITCH(1800,newio);
#endif
		SPEED_SWITCH(2400,newio);
		SPEED_SWITCH(4800,newio);
		SPEED_SWITCH(9600,newio);
		SPEED_SWITCH(19200,newio);
		SPEED_SWITCH(38400,newio);
		SPEED_SWITCH(115200,newio);
	}

	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

static int set_raw_input( struct port_descriptor* desc ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

#ifdef _WIN32
	newio.fBinary = TRUE;
	newio.fParity = TRUE;
	newio.fOutxCtsFlow = FALSE;
	newio.fOutxDsrFlow = FALSE;
	newio.fDtrControl = DTR_CONTROL_DISABLE;
	newio.fDsrSensitivity = FALSE;
	newio.fOutX = FALSE;
	newio.fInX = FALSE;
	newio.fNull = FALSE;
	newio.fRtsControl = FALSE;
	
	//Set the timeouts
	{
		COMMTIMEOUTS timeouts = {0};
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;
		if( SetCommTimeouts( desc->port, &timeouts ) == 0 ){
		printf("bad timeout\n"); fflush(stdout);}
	}
#else
	newio.c_iflag |= IGNBRK;
	newio.c_iflag &= ~BRKINT;
	newio.c_iflag &= ~ICRNL;
	newio.c_oflag = 0;
	newio.c_lflag = 0;
	newio.c_cc[VTIME] = 0;
	newio.c_cc[VMIN] = 1;
#endif
	
	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

/**
 * @param data_bits The number of data bits
 */
static int set_data_bits( struct port_descriptor* desc, int data_bits ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

#ifdef _WIN32 
	newio.ByteSize = data_bits;
#else
	newio.c_cflag &= ~CSIZE;
	if( data_bits == 8 ){
		newio.c_cflag |= CS8;
	}else if( data_bits == 7 ){
		newio.c_cflag |= CS7;
	}else if( data_bits == 6 ){
		newio.c_cflag |= CS6;
	}else if( data_bits == 5 ){
		newio.c_cflag |= CS5;
	}
#endif
	
	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

/**
 * @param stop_bits 1 for 1, 2 for 2
 */
static int set_stop_bits( struct port_descriptor* desc, int stop_bits ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

#ifdef _WIN32 
	if( stop_bits == 1 ){
		newio.StopBits = ONESTOPBIT;
	}else if( stop_bits == 2 ){
		newio.StopBits = TWOSTOPBITS;
	}
#else
	if( stop_bits == 1 ){
		newio.c_cflag &= ~CSTOPB;
	}else if( stop_bits == 2 ){
		newio.c_cflag |= CSTOPB;
	}
#endif
	
	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

/**
 * @param parity 0 for no parity, 1 for odd parity, 2 for even parity
 */
static int set_parity( struct port_descriptor* desc, int parity ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

#ifdef _WIN32 
	if( parity == 0 ){
		newio.Parity = NOPARITY;
	}else if( parity == 1 ){
		newio.Parity = ODDPARITY;
	}else if( parity == 2 ){
		newio.Parity = EVENPARITY;
	}
#else
	newio.c_iflag &= ~IGNPAR; 
	newio.c_cflag &= ~( PARODD | PARENB );
	if( parity == 0 ){
		newio.c_iflag |= IGNPAR;
	}else if( parity == 1 ){
		newio.c_cflag |= PARODD;
	}else if( parity == 2 ){
		newio.c_cflag |= PARENB;
	}
#endif
	
	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

/**
 * @param flow_control 0 for none, 1 for hardware, 2 for software
 */
static int set_flow_control( struct port_descriptor* desc, int flow_control ){
	GET_SERIAL_PORT_STRUCT( desc->port, newio );

#ifdef _WIN32
	if( flow_control == 0 ){
		newio.fOutxCtsFlow = FALSE;
		newio.fRtsControl = FALSE;
		newio.fOutX = FALSE;
		newio.fInX = FALSE;
	}else if( flow_control == 1 ){
		newio.fOutxCtsFlow = TRUE;
		newio.fRtsControl = TRUE;
		newio.fOutX = FALSE;
		newio.fInX = FALSE;
	}else if( flow_control == 2 ){
		newio.fOutxCtsFlow = FALSE;
		newio.fRtsControl = FALSE;
		newio.fOutX = TRUE;
		newio.fInX = TRUE;
	}
#else
	newio.c_iflag &= ~( IXON | IXOFF | IXANY );
	newio.c_cflag &= ~HW_FLOW;
	if( flow_control == 0 ){
		newio.c_iflag &= ~( IXON | IXOFF | IXANY );
	}else if( flow_control == 1 ){
		newio.c_cflag |= HW_FLOW;
	}else if( flow_control == 2 ){
		newio.c_iflag |= ( IXON | IXOFF | IXANY );
	}
#endif
	
	SET_SERIAL_PORT_STRUCT( desc->port, newio );

	return 1;
}

static void throw_io_exception( JNIEnv * env, int errorNumber ){
#ifdef _WIN32
	LPTSTR error_text = NULL;
	jclass exception_class;
	(*env)->ExceptionDescribe( env );
	(*env)->ExceptionClear( env );
	exception_class = (*env)->FindClass(env, "java/io/IOException");
	
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorNumber,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&error_text,
		0,
		NULL );
	(*env)->ThrowNew(env, exception_class, error_text );
	LocalFree( error_text );
#else
	jclass exception_class;
	(*env)->ExceptionDescribe( env );
	(*env)->ExceptionClear( env );
	exception_class = (*env)->FindClass(env, "java/io/IOException");
	(*env)->ThrowNew(env, exception_class, strerror( errorNumber ) );
#endif /* _WIN32 */
}

static void throw_io_exception_message( JNIEnv * env, const char* message ){
	jclass exception_class;
	(*env)->ExceptionDescribe( env );
	(*env)->ExceptionClear( env );
	exception_class = (*env)->FindClass(env, "java/io/IOException");
	(*env)->ThrowNew(env, exception_class, message );
}

static struct port_descriptor* get_port_descriptor( JNIEnv* env, jobject obj ){
	int array_pos;
	struct port_descriptor* desc;
	
	array_pos = get_handle( env, obj ); 
	if( array_pos < 0 || array_pos > port_list_size ){ 
		throw_io_exception_message( env, "Unable to get handle" );
		return NULL; 
	} 
	desc = port_list[ array_pos ]; 
	if( desc == NULL ){ 
		throw_io_exception_message( env, "Unable to get descriptor" ); 
		return NULL; 
	}
	
	return desc;
}

//
// JNI Methods
//

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    openPort
 * Signature: (Ljava/lang/String;IIIII)I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_openPort
  (JNIEnv * env, jobject obj, jstring port, jint baudRate, jint dataBits, jint stopBits, jint parity, jint flowControl){
	struct port_descriptor* new_port;
	int list_pos;
	int found = 0;
	const char* port_to_open;
	jboolean iscopy;

	port_to_open = (*env)->GetStringUTFChars( env, port, &iscopy );

	if( port_list == NULL ){
		log_message( MESSAGE_TRACE, env, "First port opened: allocating 10 port_descriptors" );
		port_list = malloc( sizeof( struct port_descriptor* ) * 10 );
		port_list_size = 10;
		for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
			port_list[ list_pos ] = NULL;
		}
	}

	//Search thru the port_list, find the first one that is NULL
	for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
		if( port_list[ list_pos ] == NULL ){
			found = 1;
			break;
		}
	}

	if( !found ){
		//no free slots.  Expand our array by 10 elements
		struct port_descriptor** tmpPortDesc;
		tmpPortDesc = malloc( sizeof( struct port_descriptor* ) * ( port_list_size + 10 ) );

		log_message( MESSAGE_TRACE, env, "No free slots found: expanding port_descirptor array by 10 elements" );

		//put all elements into the new array
		for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
			tmpPortDesc[ list_pos ] = port_list[ list_pos ];
		}
		++list_pos; //put the place to insert the new record one past the old records

		port_list_size += 10;
	
		//free the old array, set it to the new one
		free( port_list );
		port_list = tmpPortDesc;
	}

	//at this point, list_pos points to a NULL location in our array
	new_port = malloc( sizeof( struct port_descriptor ) );

	//Now, let's get to the actual opening of our port
#ifdef _WIN32
	new_port->winDTR = 0;
	new_port->winRTS = 0;
	{
		//GOD DAMN IT WINDOWS http://support.microsoft.com/kb/115831
		char* special_port = malloc( strlen( port_to_open ) + 5 );
		memcpy( special_port, "\\\\.\\", 4 );
		memcpy( special_port + 4, port_to_open, strlen( port_to_open ) + 1 );
		new_port->port = CreateFile( special_port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0 );
		free( special_port );
	}
	if( new_port->port == INVALID_HANDLE_VALUE ){
		if( GetLastError() == ERROR_FILE_NOT_FOUND ){
			LPTSTR error_text = NULL;
			jclass exception_class;
			
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&error_text,
				0,
				NULL );
		
			(*env)->ExceptionDescribe( env );
			(*env)->ExceptionClear( env );
			exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
			(*env)->ThrowNew(env, exception_class, error_text );
			free( new_port );
			LocalFree( error_text );
			return -1; 
		}
	}
	
	{
		//Let's check to see if this is a serial port
		DCB io_name = {0};
		io_name.DCBlength = sizeof( io_name ); 
		if (!GetCommState( new_port->port, &io_name ) ) { 
			LPTSTR error_text = NULL;
			jclass exception_class;
			(*env)->ExceptionDescribe( env );
			(*env)->ExceptionClear( env );
			exception_class = (*env)->FindClass(env, "com/rm5248/serial/NotASerialPortException");
			(*env)->ThrowNew(env, exception_class, "You are attempting to open something which is not a serial port" );
			free( new_port );
			return -1;
		}
	}
	
	//initialize the mutex
	new_port->in_use = CreateMutex( NULL, FALSE, NULL );
	if( new_port->in_use == NULL ){
		LPTSTR error_text = NULL;
		jclass exception_class;
		(*env)->ExceptionDescribe( env );
		(*env)->ExceptionClear( env );
		exception_class = (*env)->FindClass(env, "com/rm5248/serial/NotASerialPortException");
		(*env)->ThrowNew(env, exception_class, "UNABLE TO CREATE MUTEX(use better msg)" );
		free( new_port );
		return -1;
	}
	
#else
	pthread_mutex_init( &(new_port->in_use), NULL );
	new_port->port = open( port_to_open, O_RDWR );
	if( new_port->port < 0 && errno == ENOENT ){
		//That's not a valid serial port, error out
		jclass exception_class;
		(*env)->ExceptionDescribe( env );
		(*env)->ExceptionClear( env );
		exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
		(*env)->ThrowNew(env, exception_class, strerror( errno ) );
		free( new_port );
		return -1; 
	}else if( new_port->port < 0 ){
		jclass exception_class;
		(*env)->ExceptionDescribe( env );
		(*env)->ExceptionClear( env );
		exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
		(*env)->ThrowNew(env, exception_class, strerror( errno ) );
		free( new_port );
		return -1;
	}
	
	{
		struct termios io_name; 
		if( tcgetattr( new_port->port, &io_name ) < 0 ){ 
			if( errno == ENOTTY ){
				//This is apparently not a serial port
				jclass exception_class;
				(*env)->ExceptionDescribe( env );
				(*env)->ExceptionClear( env );
				exception_class = (*env)->FindClass(env, "com/rm5248/serial/NotASerialPortException");
				(*env)->ThrowNew(env, exception_class, "You are attempting to open something which is not a serial port" );
				free( new_port );
				return -1;
			}
		}
	}
#endif /* __WIN32 */


	
	//Set the baud rate
	if( set_baud_rate( new_port, baudRate ) < 0 ){
			throw_io_exception_message( env, "Unable to set baud rate" );
			return 0;
	}
	set_raw_input( new_port );
	//Set the data bits( character size )
	set_data_bits( new_port, dataBits );
	//Set the stop bits
	set_stop_bits( new_port, stopBits );
	//Set the parity
	set_parity( new_port, parity );
	//Set the flow control
	set_flow_control( new_port, flowControl );

	//Only set the new_port to be in our array as the last instruction
	//If there are any errors, we will have returned long before this
	port_list[ list_pos ] = new_port;

	return list_pos;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    openPort
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_openPort__Ljava_lang_String_2
  (JNIEnv * env, jobject obj, jstring port){
	struct port_descriptor* new_port;
	int list_pos;
	int found = 0;
	const char* port_to_open;
	jboolean iscopy;

	port_to_open = (*env)->GetStringUTFChars( env, port, &iscopy );

	if( port_list == NULL ){
		log_message( MESSAGE_TRACE, env, "First port opened: allocating 10 port_descriptors" );
		port_list = malloc( sizeof( struct port_descriptor* ) * 10 );
		port_list_size = 10;
		for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
			port_list[ list_pos ] = NULL;
		}
	}

	//Search thru the port_list, find the first one that is NULL
	for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
		if( port_list[ list_pos ] == NULL ){
			found = 1;
			break;
		}
	}

	if( !found ){
		//no free slots.  Expand our array by 10 elements
		struct port_descriptor** tmpPortDesc;
		tmpPortDesc = malloc( sizeof( struct port_descriptor* ) * ( port_list_size + 10 ) );

		log_message( MESSAGE_TRACE, env, "No free slots found: expanding port_descirptor array by 10 elements" );

		//put all elements into the new array
		for( list_pos = 0; list_pos < port_list_size; ++list_pos ){
			tmpPortDesc[ list_pos ] = port_list[ list_pos ];
		}
		++list_pos; //put the place to insert the new record one past the old records

		port_list_size += 10;
	
		//free the old array, set it to the new one
		free( port_list );
		port_list = tmpPortDesc;
	}

	//at this point, list_pos points to a NULL location in our array
	new_port = malloc( sizeof( struct port_descriptor ) );

	//Now, let's get to the actual opening of our port
#ifdef _WIN32
	{
		char* special_port = malloc( strlen( port_to_open ) + 5 );
		memcpy( special_port, "\\\\.\\", 4 );
		memcpy( special_port + 4, port_to_open, strlen( port_to_open ) + 1 );
		new_port->port = CreateFile( special_port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0 );
		free( special_port );
	}
	if( new_port->port == INVALID_HANDLE_VALUE ){
		if( GetLastError() == ERROR_FILE_NOT_FOUND ){
			LPTSTR error_text = NULL;
			jclass exception_class;
			
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&error_text,
				0,
				NULL );
		
			(*env)->ExceptionDescribe( env );
			(*env)->ExceptionClear( env );
			exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
			(*env)->ThrowNew(env, exception_class, error_text );
			free( new_port );
			LocalFree( error_text );
			return -1; 
		}
	}
	
	{
		//Let's check to see if this is a serial port
		DCB io_name = {0};
		io_name.DCBlength = sizeof( io_name ); 
		if (!GetCommState( new_port->port, &io_name ) ) { 
			LPTSTR error_text = NULL;
			jclass exception_class;
			(*env)->ExceptionDescribe( env );
			(*env)->ExceptionClear( env );
			exception_class = (*env)->FindClass(env, "com/rm5248/serial/NotASerialPortException");
			(*env)->ThrowNew(env, exception_class, "You are attempting to open something which is not a serial port" );
			free( new_port );
			return -1;
		}
	}
#else
	pthread_mutex_init( &(new_port->in_use), NULL );
	new_port->port = open( port_to_open, O_RDWR );
	if( new_port->port < 0 && errno == ENOENT ){
		//That's not a valid serial port, error out
		jclass exception_class;
		(*env)->ExceptionDescribe( env );
		(*env)->ExceptionClear( env );
		exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
		(*env)->ThrowNew(env, exception_class, strerror( errno ) );
		free( new_port );
		return -1; 
	}else if( new_port->port < 0 ){
		jclass exception_class;
		(*env)->ExceptionDescribe( env );
		(*env)->ExceptionClear( env );
		exception_class = (*env)->FindClass(env, "com/rm5248/serial/NoSuchPortException");
		(*env)->ThrowNew(env, exception_class, strerror( errno ) );
		free( new_port );
		return -1;
	}
	
	{
		struct termios io_name; 
		if( tcgetattr( new_port->port, &io_name ) < 0 ){ 
			if( errno == ENOTTY ){
				//This is apparently not a serial port
				jclass exception_class;
				(*env)->ExceptionDescribe( env );
				(*env)->ExceptionClear( env );
				exception_class = (*env)->FindClass(env, "com/rm5248/serial/NotASerialPortException");
				(*env)->ThrowNew(env, exception_class, "You are attempting to open something which is not a serial port" );
				free( new_port );
				return -1;
			}
		}
	}
#endif /* __WIN32 */

	
	//Only set the new_port to be in our array as the last instruction
	//If there are any errors, we will have returned long before this
	port_list[ list_pos ] = new_port;

	return list_pos;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    doClose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rm5248_serial_SerialPort_doClose
  (JNIEnv * env, jobject obj){
	//Note: We can't use get_serial_port_struct here, beacuse we need to set
	//the position in the array to NULL
	int array_pos;
	struct port_descriptor* desc;
	
	array_pos = get_handle( env, obj ); 
	if( array_pos < 0 || array_pos > port_list_size ){ 
		throw_io_exception_message( env, "Unable to get handle" );
		return; 
	} 
	desc = port_list[ array_pos ]; 
	if( desc == NULL ){ 
		throw_io_exception_message( env, "Unable to get descriptor" ); 
		return; 
	}

#ifdef _WIN32
	{
		HANDLE tmpHandle = desc->port;
		desc->port = INVALID_HANDLE_VALUE;
		CloseHandle( tmpHandle );
	}
	WaitForSingleObject( desc->in_use, INFINITE );
	ReleaseMutex( desc->in_use );
	CloseHandle( desc->in_use );
#else
	{
		int tmpFd = desc->port;
		desc->port = -1;
		close( tmpFd );
	}
	pthread_mutex_lock( &(desc->in_use) );
	pthread_mutex_unlock( &(desc->in_use) );
#endif

	free( port_list[ array_pos ] );
	port_list[ array_pos ] = NULL;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setBaudRate
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rm5248_serial_SerialPort_setBaudRate
  (JNIEnv * env, jobject obj, jint baud_rate ){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );

	if( desc == NULL ){
		return 0;
	}

	return set_baud_rate( desc, baud_rate );
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getBaudRateInternal
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getBaudRateInternal
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	//Now, let's get the baud rate information
	{
		GET_SERIAL_PORT_STRUCT( desc->port, newio );
#ifdef _WIN32
		GetCommState( desc->port, &newio );
		switch( newio.BaudRate ){
#else
		switch( cfgetispeed( &newio ) ){
		GET_SPEED_SWITCH( 0, newio );
		GET_SPEED_SWITCH( 50, newio );
		GET_SPEED_SWITCH( 75, newio );
#endif /* _WIN32 */
		GET_SPEED_SWITCH( 110, newio );
#ifndef _WIN32
		GET_SPEED_SWITCH( 134, newio );
		GET_SPEED_SWITCH( 150, newio );
		GET_SPEED_SWITCH( 200, newio );
#endif /* _WIN32 */
		GET_SPEED_SWITCH( 300, newio );
		GET_SPEED_SWITCH( 600, newio );
		GET_SPEED_SWITCH( 1200, newio );
#ifndef _WIN32
		GET_SPEED_SWITCH( 1800, newio );
#endif /* _WIN32 */
		GET_SPEED_SWITCH( 2400, newio );
		GET_SPEED_SWITCH( 4800, newio );
		GET_SPEED_SWITCH( 9600, newio );
		GET_SPEED_SWITCH( 19200, newio );
		GET_SPEED_SWITCH( 38400, newio );
		GET_SPEED_SWITCH( 115200, newio );
		default:
			return 0;
		} /* end switch */
	}

}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setStopBits
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rm5248_serial_SerialPort_setStopBits
  (JNIEnv * env, jobject obj, jint bits){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	return set_stop_bits( desc, bits );
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getStopBitsInternal
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getStopBitsInternal
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	{
		GET_SERIAL_PORT_STRUCT( desc->port, newio );
#ifdef _WIN32
		if( newio.StopBits == 1 ){
			return 1;
		}else if( newio.StopBits == 2 ){
			return 2;
		}else{
			return -1;
		}
#else
		if( newio.c_cflag & CSTOPB ){
			return 2;
		}else{
			return 1;
		}
#endif
	}
}
/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setCharSize
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rm5248_serial_SerialPort_setCharSize
  (JNIEnv * env, jobject obj, jint size){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	return set_data_bits( desc, size );
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getCharSizeInternal
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getCharSizeInternal
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	//Now get the char size
	{
		GET_SERIAL_PORT_STRUCT( desc->port, newio );
		
#ifdef _WIN32
		return newio.ByteSize;
#else
		if( ( newio.c_cflag | CS8 ) == CS8 ){
			return 8;
		}else if( ( newio.c_cflag | CS7 ) == CS7 ){
			return 7;
		}else if( ( newio.c_cflag | CS6 ) == CS6 ){
			return 6;
		}else if( ( newio.c_cflag | CS5 ) == CS5 ){
			return 5;
		}else{
			return 0;
		}
#endif
	}

}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setParity
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rm5248_serial_SerialPort_setParity
  (JNIEnv * env, jobject obj, jint parity){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}	
	
	return set_parity( desc, parity );
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getParityInternal
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getParityInternal
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	{
		GET_SERIAL_PORT_STRUCT( desc->port, newio );
#ifdef _WIN32
		if( newio.Parity == NOPARITY ){
			return 0;
		}else if( newio.Parity == ODDPARITY ){
			return 1;
		}else if( newio.Parity == EVENPARITY ){
			return 2;
		}else{
			return -1;
		}
#else
		if( !( newio.c_cflag & PARENB ) ){
			//No parity
			return 0;
		}else if( newio.c_cflag & PARODD ){
			//Odd parity
			return 1;
		}else if( !( newio.c_cflag & PARODD ) ){
			//Even parity
			return 2;
		}else{
			return -1;
		}
#endif
	}

}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setFlowControl
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rm5248_serial_SerialPort_setFlowControl
  (JNIEnv * env, jobject obj, jint flow){
	struct port_descriptor* desc;
	
	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	return set_flow_control( desc, flow );
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getFlowControlInternal
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getFlowControlInternal
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	{
		GET_SERIAL_PORT_STRUCT( desc->port, newio );
#ifdef _WIN32
		if( newio.fOutX == TRUE && newio.fInX == TRUE ){
			return 2;
		}else if( newio.fRtsControl == TRUE && newio.fOutxCtsFlow == TRUE ){
			return 1;
		}else{
			return 0;
		}
#else
		if( newio.c_cflag & ~( IXON ) &&
			newio.c_cflag & ~( IXOFF ) &&
			newio.c_cflag & ~( IXANY ) ){
			return 0;
		}else if( newio.c_cflag & HW_FLOW ){
			return 1;
		}else if( newio.c_cflag & ( IXON ) &&
			newio.c_cflag & ( IXOFF ) &&
			newio.c_cflag & ( IXANY ) ){
			return 2;
		}
#endif /* _WIN32 */
	}

	return -1;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getSerialLineStateInternalNonblocking
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getSerialLineStateInternalNonblocking
  (JNIEnv * env, jobject obj ){
	struct port_descriptor* desc;
	jint ret_val;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	ret_val = 0;
	
	{	
#ifdef _WIN32
		DWORD get_val;
		if( GetCommModemStatus( desc->port, &get_val ) == 0 ){
			throw_io_exception( env, GetLastError() );
			return -1;
		}
		
		if( get_val & MS_CTS_ON ){
			// CTS
			ret_val |= ( 0x01 << 1 );
		}
		
		if( get_val & MS_DSR_ON ){
			// Data Set Ready
			ret_val |= ( 0x01 << 2 );
		}
		
		if( desc->winDTR ){
			ret_val |= ( 0x01 << 3 );
		}
		
		if( desc->winRTS ){
			ret_val |= ( 0x01 << 4 );
		}
		
		if( get_val & MS_RING_ON ){
			// Ring Indicator
			ret_val |= ( 0x01 << 5 );
		}
#else
		int get_val;
		if( ioctl( desc->port, TIOCMGET, &get_val ) < 0 ){
			throw_io_exception( env, errno );
			return -1;
		}

		if( get_val & TIOCM_CD ){
			// Carrier detect
			ret_val |= 0x01;
		}

		if( get_val & TIOCM_CTS ){
			// CTS
			ret_val |= ( 0x01 << 1 );
		}

		if( get_val & TIOCM_DSR ){
			// Data Set Ready
			ret_val |= ( 0x01 << 2 );
		}

		if( get_val & TIOCM_DTR ){
			// Data Terminal Ready
			ret_val |= ( 0x01 << 3 );
		}

		if( get_val & TIOCM_RTS ){
			// Request To Send
			ret_val |= ( 0x01 << 4 );
		}

		if( get_val & TIOCM_RI ){
			// Ring Indicator
			ret_val |= ( 0x01 << 5 );
		}
#endif
	}

	return ret_val;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    setSerialLineStateInternal
 * Signature: (Lcom/rm5248/serial/SerialLineState;)I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_setSerialLineStateInternal
  (JNIEnv * env, jobject obj, jobject serial){
	struct port_descriptor* desc;
	jint ret_val;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

	ret_val = 0;
	
#ifdef _WIN32
	if( get_bool( env, serial, "dataTerminalReady" ) ){
		if( !EscapeCommFunction( desc->port, SETDTR ) ){
			throw_io_exception_message( env, "Could not set DTR" );
			return -1;
		}
		desc->winDTR = 1;
	}else{
		if( !EscapeCommFunction( desc->port, CLRDTR ) ){
			throw_io_exception_message( env, "Could not set DTR" );
			return -1;
		}
		desc->winDTR = 0;
	}

	if( get_bool( env, serial, "requestToSend" ) ){
		if( !EscapeCommFunction( desc->port, SETRTS ) ){
			throw_io_exception_message( env, "Could not set RTS" );
			return -1;
		}
		desc->winRTS = 1;
	}else{
		if( !EscapeCommFunction( desc->port, CLRRTS ) ){
			throw_io_exception_message( env, "Could not set RTS" );
			return -1;
		}
		desc->winRTS = 0;
	}
#else
	int toSet = 0;

	if( ioctl( desc->port, TIOCMGET, &toSet ) < 0 ){
		throw_io_exception_message( env, "Could not get port settings" );
		return -1;
	}

	if( get_bool( env, serial, "dataTerminalReady" ) ){
		toSet |= TIOCM_DTR;
	}else{
		toSet &= ~TIOCM_DTR;
	}

	if( get_bool( env, serial, "requestToSend" ) ){
		toSet |= TIOCM_RTS;
	}else{
		toSet &= ~TIOCM_RTS;
	}

	if( ioctl( desc->port, TIOCMSET, &toSet ) < 0 ){
		throw_io_exception_message( env, "Could not set port settings" );
	}
#endif

	return ret_val;
}

//
// ------------------------------------------------------------------------
// ------------------Input/Output methods below here-----------------------
// ------------------------------------------------------------------------
//


/*
 * Class:     com_rm5248_serial_SerialInputStream
 * Method:    readByte
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialInputStream_readByte
  (JNIEnv * env, jobject obj){
	int stat;
	int ret_val;
	struct port_descriptor* desc;
	int get_val = 0;
#ifdef _WIN32
	DWORD ret = 0;
	OVERLAPPED overlap = {0};
	int current_available = 0;
#endif 

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	ret_val = 0;

#ifdef _WIN32
	{
		DWORD comErrors = {0};
		COMSTAT portStatus = {0};
		if( !ClearCommError( desc->port, &comErrors, &portStatus ) ){
			//return value zero = fail
			throw_io_exception( env, GetLastError() );
			return -1;
		}else{
			current_available = portStatus.cbInQue;
		}
	}
	
	WaitForSingleObject( desc->in_use, INFINITE );
	if( !current_available ){
		//If nothing is currently available, wait until we get an event of some kind.
		//This could be the serial lines changing state, or it could be some data
		//coming into the system.
		overlap.hEvent = CreateEvent( 0, TRUE, 0, 0 );
		SetCommMask( desc->port, EV_RXCHAR | EV_CTS | EV_DSR | EV_RING );
		WaitCommEvent( desc->port, &ret, &overlap );
		WaitForSingleObject( overlap.hEvent, INFINITE );
	}else{
		//Data is available; set the RXCHAR mask so we try to read from the port
		ret = EV_RXCHAR;
	}
		
	if( ret & EV_RXCHAR ){
		if( !ReadFile( desc->port, &ret_val, 1, &stat, &overlap) ){
			throw_io_exception( env, GetLastError() );
			ReleaseMutex( desc->in_use );
			return -1;
		}
		
		//This is a valid byte, set our valid bit
		ret_val |= ( 0x01 << 15 );
	}else if( ret == 0 && desc->port == INVALID_HANDLE_VALUE ){
		//the port was closed
		ReleaseMutex( desc->in_use );
		return -1;
	}
	
	//Always get the com lines no matter what	
	if( GetCommModemStatus( desc->port, &get_val ) == 0 ){
		throw_io_exception( env, GetLastError() );
		return -1;
	}
		
	if( get_val & MS_CTS_ON ){
		// CTS
		ret_val |= ( 0x01 << 10 );
	}
		
	if( get_val & MS_DSR_ON ){
		// Data Set Ready
		ret_val |= ( 0x01 << 11 );
	}
	
	if( desc->winDTR ){
		ret_val |= ( 0x01 << 12 );
	}
	
	if( desc->winRTS ){
		ret_val |= ( 0x01 << 13 );
	}
		
	if( get_val & MS_RING_ON ){
		// Ring Indicator
		ret_val |= ( 0x01 << 14 );
	}
	
	ReleaseMutex( desc->in_use );
	
#else

	//Okay, this here is going to be a bit ugly.
	//The problem here is that Linux/POSIX don't specify that TIOCMIWAIT
	//has to exist.  Also, the fact that if we use TIOCMIWAIT we don't
	//timeout or anything.  What would be very convenient in this case
	//would be to have select() return when the state changed, but
	//it's not possible. :(
	//Ironically, this is one case in which the Windows serial API
	//is better than the POSIX way.
	fd_set fdset;
	struct timeval timeout;
	int originalState;
	int selectStatus;

	pthread_mutex_lock( &(desc->in_use) );

	//first get the original state of the serial port lines
	if( ioctl( desc->port, TIOCMGET, &originalState ) < 0 ){
		throw_io_exception( env, errno );
		pthread_mutex_unlock( &(desc->in_use) );
		return -1;
	}
	
	while( 1 ){
		if( desc->port == -1 ){
			pthread_mutex_unlock( &(desc->in_use) );
			return -1;
		}
		FD_ZERO( &fdset );
		FD_SET( desc->port, &fdset );
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000; // 10,000 microseconds = 10ms

		selectStatus = select( desc->port + 1, &fdset, NULL, NULL, &timeout );
		if( desc->port == -1 ){
			//check to see if the port is closed
			pthread_mutex_unlock( &(desc->in_use) );
			return -1;
		}

		if( selectStatus < 0 ){
			int errval;
			if( errno == EBADF ){
				// EOF
				errval= 0;
			}else{
				throw_io_exception( env, errno );
				errval = -1;
			}
			pthread_mutex_unlock( &(desc->in_use) );
			return errval;
		}

		if( selectStatus == 0 ){
			//This was a timeout
			if( ioctl( desc->port, TIOCMGET, &get_val ) < 0 ){
				throw_io_exception( env, errno );
				pthread_mutex_unlock( &(desc->in_use) );
				return -1;
			}

			if( get_val == originalState ){
				//The state of the lines have not changed, 
				//continue on until something changes
				continue;
			}
			
		}
	
		if( FD_ISSET( desc->port, &fdset ) ){
			stat = read( desc->port, &ret_val, 1 );
			if( stat < 0 ){
				//throw new exception
				throw_io_exception( env, errno );
				pthread_mutex_unlock( &(desc->in_use) );
				return -1;
			}
		
			//This is a valid byte, set our valid bit
			ret_val |= ( 0x01 << 15 );
		}
		
		//We get to this point if we either 1. have data or
		//2. our state has changed
		break;
	}

	//Now that we have read in the character, let's get the serial port line state.
	//If it has changed, we will fire an event in Java.
	//Now, because we only read one byte at a time, we will use the lower 8 bytes to 
	//return the character that we read.  The other bytes will be used to return
	//information on our serial port state.
	if( ioctl( desc->port, TIOCMGET, &get_val ) < 0 ){
		throw_io_exception( env, errno );
		pthread_mutex_unlock( &(desc->in_use) );
		return -1;
	}

	if( get_val & TIOCM_CD ){
		// Carrier detect
		ret_val |= ( 0x01 << 9 );
	}

	if( get_val & TIOCM_CTS ){
		// CTS
		ret_val |= ( 0x01 << 10 );
	}

	if( get_val & TIOCM_DSR ){
		// Data Set Ready
		ret_val |= ( 0x01 << 11 );
	}

	if( get_val & TIOCM_DTR ){
		// Data Terminal Ready
		ret_val |= ( 0x01 << 12 );
	}
		
	if( get_val & TIOCM_RTS ){
		// Request To Send
		ret_val |= ( 0x01 << 13 );
	}
	
	if( get_val & TIOCM_RI ){
		// Ring Indicator
		ret_val |= ( 0x01 << 14 );
	}
	
	pthread_mutex_unlock( &(desc->in_use) );
#endif

	return ret_val;
}

/*
 * Class:     com_rm5248_serial_SerialInputStream
 * Method:    getAvailable
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialInputStream_getAvailable
  (JNIEnv * env, jobject obj){
	jint ret_val;
	struct port_descriptor* desc;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}

#ifdef _WIN32
	{
		DWORD comErrors = {0};
		COMSTAT portStatus = {0};
		if( !ClearCommError( desc->port, &comErrors, &portStatus ) ){
			//return value zero = fail
			throw_io_exception( env, GetLastError() );
			return -1;
		}else{
			ret_val = portStatus.cbInQue;
		}
	}
#else
	if( ioctl( desc->port, FIONREAD, &ret_val ) < 0 ){
		//throw new exception
		throw_io_exception( env, errno );
		return -1;
	}
#endif
	return ret_val;
}

/*
 * Class:     com_rm5248_serial_SimpleSerialInputStream
 * Method:    readByte
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SimpleSerialInputStream_readByte
  (JNIEnv * env, jobject obj){
	struct port_descriptor* desc;
	jint ret_val;
	int stat;
#ifdef _WIN32
	DWORD ret = 0;
	OVERLAPPED overlap = {0};
	int current_available = 0;
#endif 

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return 0;
	}
	
	ret_val = 0;

#ifdef _WIN32
	WaitForSingleObject( desc->in_use, INFINITE );

	{
		DWORD comErrors = {0};
		COMSTAT portStatus = {0};
		if( !ClearCommError( desc->port, &comErrors, &portStatus ) ){
			//return value zero = fail
			throw_io_exception( env, GetLastError() );
			ReleaseMutex( desc->in_use );
			return -1;
		}else{
			current_available = portStatus.cbInQue;
		}
	}
	
	if( !current_available ){
		//If nothing is currently available, wait until we get an event of some kind.
		//This could be the serial lines changing state, or it could be some data
		//coming into the system.
		overlap.hEvent = CreateEvent( 0, TRUE, 0, 0 );
		SetCommMask( desc->port, EV_RXCHAR );
		WaitCommEvent( desc->port, &ret, &overlap );
		WaitForSingleObject( overlap.hEvent, INFINITE );
	}else{
		//Data is available; set the RXCHAR mask so we try to read from the port
		ret = EV_RXCHAR;
	}
		
	if( ret & EV_RXCHAR ){
		if( !ReadFile( desc->port, &ret_val, 1, &stat, &overlap) ){
			throw_io_exception( env, GetLastError() );
			ReleaseMutex( desc->in_use );
			return -1;
		}
	}else if( ret == 0 && desc->port == INVALID_HANDLE_VALUE ){
		//the port was closed
		ret_val = -1;
	}else if( ret == 0 ){
		//some unknown error occured
		throw_io_exception( env, GetLastError() );
		ret_val = -1;
	}
	
	ReleaseMutex( desc->in_use );
#else
	pthread_mutex_lock( &(desc->in_use) );
	//do a polll() on the FD.
	//we need to do this so that if we close() our FD from a different thread,
	//this function will actually return.
	do{
		struct pollfd pollfds;
		pollfds.fd = desc->port;
		pollfds.events = POLLIN | POLLERR | POLLNVAL;
		stat = poll( &pollfds, 1, 100 );
		if( stat < 0 ){
			throw_io_exception( env, errno );
			pthread_mutex_unlock( &(desc->in_use) );
			return -1;
		}else if( stat > 0 ){
			break;
		}
	}while( 1 );


	if( desc->port == -1 ){
		//EOF
		pthread_mutex_unlock( &(desc->in_use) );
		return -1;
	}

	stat = read( desc->port, &ret_val, 1 );
	if( stat < 0 ){
		throw_io_exception( env, errno );
		pthread_mutex_unlock( &(desc->in_use) );
		return -1;
	}
	pthread_mutex_unlock( &(desc->in_use) );
#endif
	return ret_val;
}

/*
 * Class:     com_rm5248_serial_SimpleSerialInputStream
 * Method:    getAvailable
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SimpleSerialInputStream_getAvailable
  (JNIEnv * env, jobject obj){
	//use our already-existing method to get the available bytes, it already works
	return Java_com_rm5248_serial_SerialInputStream_getAvailable( env, obj );
}


/*
 * Class:     com_rm5248_serial_SerialOutputStream
 * Method:    writeByte
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rm5248_serial_SerialOutputStream_writeByte
  (JNIEnv * env, jobject obj, jint byte){
	struct port_descriptor* desc;
	char byte_write;
#ifdef _WIN32
	DWORD bytes_written;
	OVERLAPPED overlap;
#else
	int bytes_written;
#endif

	byte_write = byte;

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return;
	}

#ifdef _WIN32
	memset( &overlap, 0, sizeof( overlap ) );
	overlap.hEvent = CreateEvent( 0, TRUE, 0, 0 );
	if( !WriteFile( desc->port, &byte_write, sizeof( byte_write ), &bytes_written, &overlap ) ){
		if( GetLastError() == ERROR_IO_PENDING ){
			//Probably not an error, we're just doing this in an async fasion
			if( WaitForSingleObject( overlap.hEvent, INFINITE ) == WAIT_FAILED ){
				throw_io_exception( env, GetLastError() );
				return;
			}
		}else{
			throw_io_exception( env, GetLastError() );
			return;
		}
	}
#else
	bytes_written = write( desc->port, &byte_write, sizeof( byte_write ) );
	if( bytes_written < 0 || 
            bytes_written != sizeof( byte_write) ){
		//throw new exception
		throw_io_exception( env, errno );
		return;
	}
#endif
}

/*
 * Class:     com_rm5248_serial_SerialOutputStream
 * Method:    writeByteArray
 * Signature: ([B)V
 */
JNIEXPORT void JNICALL Java_com_rm5248_serial_SerialOutputStream_writeByteArray
  (JNIEnv * env, jobject obj, jbyteArray arr){
	jbyte* data;
	jint len;
	struct port_descriptor* desc;
#ifdef _WIN32
	DWORD bytes_written;
	OVERLAPPED overlap;
#else
	int bytes_written = 0;
	int offset = 0;
	int rc;
#endif /* _WIN32 */

	desc = get_port_descriptor( env, obj );
	if( desc == NULL ){
		return;
	}

	len = (*env)->GetArrayLength( env, arr );
	data = (*env)->GetByteArrayElements(env, arr, 0);

#ifdef _WIN32
	memset( &overlap, 0, sizeof( overlap ) );
	overlap.hEvent = CreateEvent( 0, TRUE, 0, 0 );
	if( !WriteFile( desc->port, data, len, &bytes_written, &overlap ) ){
		if( GetLastError() == ERROR_IO_PENDING ){
			//Probably not an error, we're just doing this in an async fasion
			if( WaitForSingleObject( overlap.hEvent, INFINITE ) == WAIT_FAILED ){
				throw_io_exception( env, GetLastError() );
				return;
			}
		}else{
			throw_io_exception( env, GetLastError() );
		}
	}
	
#else

	do{
		rc = write( desc->port, data + offset, len - bytes_written );
		if( rc < 0 ){
			throw_io_exception( env, errno );
			break;
		}
		bytes_written += rc;
		offset += rc;
	}while( bytes_written < len );
#endif /* _WIN32 */

	(*env)->ReleaseByteArrayElements(env, arr, data, 0);
}


//
// ------------------------------------------------------------------------
// ---------------------Static methods below here--------------------------
// ------------------------------------------------------------------------
//

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getMajorNativeVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getMajorNativeVersion
  (JNIEnv * env, jclass cls){
	return 0;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getMinorNativeVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_rm5248_serial_SerialPort_getMinorNativeVersion
  (JNIEnv * env, jclass cls){
	return 9;
}

/*
 * Class:     com_rm5248_serial_SerialPort
 * Method:    getSerialPorts
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_rm5248_serial_SerialPort_getSerialPorts
  (JNIEnv * env, jclass cls){
	jclass stringClass; 
    jobjectArray array; 
	char** port_names;
	int port_names_size;
	int x;
	
	port_names_size = 0;
	/* Note: max of 255 serial ports on windows, however mac and linux
	 * can have many more than that.  So we allocate enough for 512 
	 * ports
	 */
	port_names = malloc( sizeof( char* ) * 512 ); //max 512 serial ports
#ifdef _WIN32
	{
		//Brute force, baby!
		char* port_to_open = malloc( 11 );
		HANDLE* port;
		for( x = 0; x <= 255; x++ ){
			_snprintf_s( port_to_open, 11, 11, "\\\\.\\COM%d", x );
			port = CreateFile( port_to_open, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0 );
			if( port != INVALID_HANDLE_VALUE ||
				( port == INVALID_HANDLE_VALUE &&
				GetLastError() != ERROR_FILE_NOT_FOUND ) ){
				//This is a valid port
				//we could get INVALID_HANDLE_VALUE if the port is already open,
				//so make sure we check to see if it is not that value
				port_names[ port_names_size ] = malloc( 6 );
				memcpy( port_names[ port_names_size ], port_to_open + 4, 6 );
				port_names_size++;
			}
			CloseHandle( port );
		}
		free( port_to_open );
	}
#else
	{
		struct dirent *entry;
		DIR* dir;
		int fd;
		char* deviceName;

		deviceName = malloc( 100 );

		dir = opendir( "/dev/" );
		if( dir == NULL ){
			throw_io_exception_message( env, "We can't open /dev." );
			free( port_names );
			free( deviceName );
			return NULL;
		}
		while ( entry = readdir( dir ), entry != NULL) {
			if( snprintf( deviceName, 100, "/dev/%s", entry->d_name ) >= 99 ){
				fprintf( stderr, "WARNING: Ignoring file %s, filename too long\n", entry->d_name );
				continue;
			}
			fd = open( deviceName, O_RDONLY | O_NONBLOCK );
			if( fd < 0 ){
				switch( errno ){
					case EACCES:
					case ENOMEDIUM:
						//For some reason, I get errno 22 on my laptop
						//when opening /dev/video0, which prints out
						//"No such device or address"
						//Not adding it here, because that does seem bad
						break;
					default:
						perror( "open" );
						fprintf( stderr, "ERROR: This is a very bad thing that should never happen %s:%d  errno:%d\n", __FILE__, __LINE__, errno );
				}
				close( fd );
				continue;
			}
			
			if( isatty( fd ) && port_names_size < 512 ){
				port_names[ port_names_size ] = malloc( strlen( entry->d_name ) + 6 );
				memcpy( port_names[ port_names_size ], "/dev/", 5 );
				memcpy( port_names[ port_names_size ] + 5, entry->d_name, strlen( entry->d_name ) + 1 );
				port_names_size++;
			}
			
			close( fd );
		}
		closedir( dir );
		free( deviceName );
	}
#endif /* _WIN32 */

	stringClass = (*env)->FindClass(env, "java/lang/String");
	array = (*env)->NewObjectArray(env, port_names_size, stringClass, 0);
	
	for( x = 0; x < port_names_size; x++ ){
		(*env)->SetObjectArrayElement(env, array, x, (*env)->NewStringUTF( env, port_names[ x ] ) );
		free( port_names[ x ] );
	}
	
	free( port_names );
	
	return array;
}
