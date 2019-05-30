/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!  
 *        \file  mscan_qstest.c
 *
 *      \author  ulrich.bogensperger@men.de
 * 
 *  	 \brief  Test tool for two MSCAN controllers
 *
 *               Tests MSCAN functionality and wiring by sending
 *               some CAN frames between the two given devices.
 *
 *     Switches: -
 *     Required: libraries: mdis_api, usr_oss, usr_utl, mscan_api
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 2004-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/mdis_err.h>
#include <MEN/usr_err.h>
#include <MEN/mscan_api.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);


/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define CHK(expression) \
 if( !(expression)) {\
	 printf("\n*** Error during: %s\nfile %s\nline %d\n", \
      #expression,__FILE__,__LINE__);\
      printf("%s\n",mscan_errmsg(UOS_ErrnoGet()));\
     goto ABORT;\
 }

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);

static int LoopbBasic( MDIS_PATH path1, MDIS_PATH path2 );
static void DumpFrame( char *msg, const MSCAN_FRAME *frm );
static int CmpFrames( const MSCAN_FRAME *frm1, const MSCAN_FRAME *frm2 );
static void __MAPILIB SigHandler( u_int32 sigCode );

/*--------------------------------------+
|   TYPEDEFS                            |
+--------------------------------------*/
/* (none) */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
/* filters to let everything pass through */
static const MSCAN_FILTER G_stdOpenFilter = { 
	0,
	0xffffffff,
	0,
	0 
};
static const MSCAN_FILTER G_extOpenFilter = { 
	0,
	0xffffffff,
	MSCAN_EXTENDED,
	0 
};

static int G_sigUos1Cnt, G_sigUos2Cnt;	/* signal counters */
static int G_endMe;


/**********************************************************************/
/** Print program usage
 */
static void usage(void)
{

	printf(
        "Check CAN communication between <device1> and <device2>\n"
		"usage: mscan_qstest [<opts>] <device1> <device2> [<opts>]\n"
		"Options:\n"
		"  -b=<code>    bitrate code (0..6)              [0]\n"
		"                  0=1MBit 1=800kbit 2=500kbit 3=250kbit\n"
		"                  4=125kbit 5=100kbit 6=50kbit\n"
		"  -n=<runs>    number of runs through test      [1]\n"
		"  -s           stop on first error ............ [no]\n" );

	printf("(c) 2004 by MEN Mikro Elektronik GmbH\n%s\n", IdentString );
}

/**********************************************************************/
/** Program entry point
 * \return success (0) or error (1)
 */
int main( int argc, char *argv[] )
{
	int32	ret=1, n, error;
    MDIS_PATH path1=-1, path2=-1;
	int stopOnFirst, runs=1, run, errCount=0;
	u_int32 bitrate, spl=0;
	char	*device[2],*str,*errstr,buf[40];

	G_endMe = FALSE;
	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("n=sb=?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
    device[0] = device[1] = NULL;

	for (n=1; n<argc; n++) {
		if (*argv[n] != '-') {
            if( device[0] == NULL )
                device[0] = argv[n];
            else
                device[1] = argv[n];
		}
    }

	if (!device[0] || !device[1]) {
		usage();
		return(1);
	}

	bitrate  = ((str = UTL_TSTOPT("b=")) ? atoi(str) : 0);
	runs	 = ((str = UTL_TSTOPT("n=")) ? atoi(str) : 1);
	stopOnFirst = !!UTL_TSTOPT("s");

	UOS_SigInit( SigHandler );

	/*--------------------+
    |  open pathes        |
    +--------------------*/
	CHK( (path1 = mscan_init(device[0])) >= 0 );
	CHK( (path2 = mscan_init(device[1])) >= 0 );


	/*--------------------+
    |  config             |
    +--------------------*/
	CHK( mscan_set_bitrate( path1, (MSCAN_BITRATE)bitrate, spl ) == 0 );
	CHK( mscan_set_bitrate( path2, (MSCAN_BITRATE)bitrate, spl ) == 0 );

	/*--- config error object ---*/
	CHK( mscan_config_msg( path1, 0, MSCAN_DIR_RCV, 10, NULL ) == 0 );
	CHK( mscan_config_msg( path2, 0, MSCAN_DIR_RCV, 10, NULL ) == 0 );

	/*--- enable bus ---*/
	CHK( mscan_enable( path1, TRUE ) == 0 );
	CHK( mscan_enable( path2, TRUE ) == 0 );

    printf( "Performing test...\n" );

    for( run=1; run<=runs; run++ ){
        if( G_endMe )
            goto ABT1;

        error = LoopbBasic( path1, path2 );
        if( error ) {
            errCount++;
            printf( "Test failed. Error count=%d\n", errCount );
        }

        if( error && stopOnFirst )
            goto ABT1;
    }

 ABT1:
	printf("------------------------------------------------\n");
	printf("TEST RESULT: %d errors\n", errCount );

	ret = 0;
	CHK( mscan_enable( path1, FALSE ) == 0 );
	CHK( mscan_enable( path2, FALSE ) == 0 );
	CHK( mscan_term(path1) == 0 );
	CHK( mscan_term(path2) == 0 );
	path1 = path2 = -1;

 ABORT:
	UOS_SigExit();

	if( path1 != -1 ) {
        mscan_enable( path1, FALSE );
        mscan_enable( path2, FALSE );
		mscan_term(path1);
		mscan_term(path2);
    }

	return(ret);
}

static void __MAPILIB SigHandler( u_int32 sigCode )
{
	switch( sigCode ){
	case UOS_SIG_USR1:
		G_sigUos1Cnt++;	
		break;
	case UOS_SIG_USR2:
		G_sigUos2Cnt++;	
		break;
	default:
		G_endMe = TRUE;
	}
}


/**********************************************************************/
/** Basic Tx/Rx test 
 * 
 * Configures:
 * - one tx object
 * - two rx objects (one for standard, one for extended IDs)
 *
 * Global filter are configured to let all messages pass through
 * Rx object filters are configured to let all messages pass through
 *
 * Each frame of table \em txFrm is sent and it is checked if the frame
 * could be received correctly on the expected Rx object.
 *
 * \return 0=ok, -1=error
 */
static int LoopbBasic( MDIS_PATH path1, MDIS_PATH path2 )
{
	int i, rxObj, rv = -1;
	const int txObj = 5;
	const int rxObj1 = 1;
	const int rxObj2 = 2;

	/* frames to send */
	static const MSCAN_FRAME txFrm[] = {
		/* ID,  flags,          dlen, data */
		{ 0x12, 0,				1,   { 0xa5 } },
		{ 0x45, 0,				8,   { 0x01, 0x02, 0x03, 0x04, 0x05, 
									   0x06, 0x07, 0x08 } },
		{ 0x13218765,  MSCAN_EXTENDED, 2, { 0x99, 0xcc } },
		{ 0x55, 0,				4,   { 0xff, 0x00, 0x7f, 0x1e } },
		{ 0x124, 0,				0,   { 0 } }
	};
	MSCAN_FRAME rxFrm;

	/* Tx object */
	CHK( mscan_config_msg( path1, txObj, MSCAN_DIR_XMT, 10, NULL ) == 0 );

	/* Rx object for standard messages */
	CHK( mscan_config_msg( path2, rxObj1, MSCAN_DIR_RCV, 20, 
						   &G_stdOpenFilter ) == 0 );

	/* Rx object for extended messages */
	CHK( mscan_config_msg( path2, rxObj2, MSCAN_DIR_RCV, 20, 
						   &G_extOpenFilter ) == 0 );


	for( i=0; i<sizeof(txFrm)/sizeof(MSCAN_FRAME); i++ ){

		/* send one frame */
		CHK( mscan_write_msg( path1, txObj, 1000, &txFrm[i] ) == 0 );
		
		/* wait for frame on correct object */
		rxObj = txFrm[i].flags & MSCAN_EXTENDED ? rxObj2 : rxObj1;

		CHK( mscan_read_msg( path2, rxObj, 1000, &rxFrm ) == 0 );

		/* check if received correctly */
		if( CmpFrames( &rxFrm, &txFrm[i] ) != 0 ){
			printf("Incorrect Frame received\n");
			DumpFrame( "Sent", &txFrm[i] );
			DumpFrame( "Recv", &rxFrm );
			CHK(0);
		}
	}

    rv = 0;

 ABORT:
	mscan_config_msg( path1, txObj, MSCAN_DIR_DIS, 0, NULL );
	mscan_config_msg( path2, rxObj1, MSCAN_DIR_DIS, 0, NULL );
	mscan_config_msg( path2, rxObj2, MSCAN_DIR_DIS, 0, NULL );

	return rv;

}


static int CmpFrames( const MSCAN_FRAME *frm1, const MSCAN_FRAME *frm2 )
{
	int i;

	if( frm1->id != frm2->id )
		return -1;

	if( frm1->flags != frm2->flags )
		return -1;

	if( frm1->dataLen != frm2->dataLen )
		return -1;

	for( i=0; i<frm1->dataLen; i++ )
		if( frm1->data[i] != frm2->data[i] )
			return -1;

	return 0;
}

static void DumpFrame( char *msg, const MSCAN_FRAME *frm )
{
	int i;
	printf("%s: ID=0x%08lx%s%s data=", 
		   msg,
		   frm->id, 
		   (frm->flags & MSCAN_EXTENDED) ? "x":"", 
		   (frm->flags & MSCAN_RTR) ? "RTR":"");

	for(i=0; i<frm->dataLen; i++ ){
		printf("%02x ", frm->data[i] );
	}
	printf("\n");
}

