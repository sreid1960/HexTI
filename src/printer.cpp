/*
    HEXTIr-SD - Texas Instruments HEX-BUS SD Mass Storage Device
    Copyright Jim Brain and RETRO Innovations, 2017

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License only.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    printer.cpp: Printer-based (over Serial) device functions.
*/

#include "config.h"
#include "hexbus.h"
#include "hexops.h"
#include "timer.h"

#include "printer.h"


#ifdef INCLUDE_PRINTER
#include <Arduino.h>
#endif

// Global references
extern uint8_t buffer[BUFSIZE];

#ifdef INCLUDE_PRINTER
// Global defines
volatile uint8_t  prn_open = 0;


/*
   hex_prn_open() -
   "opens" the Serial.object for use as a printer at device code 12 (default PC-324 printer).
*/
static uint8_t hex_prn_open(pab_t pab) {
  uint16_t len = 0;
  uint8_t  rc = HEXSTAT_SUCCESS;
  uint8_t  att = 0;
  BYTE     res = HEXSTAT_SUCCESS;

  len = 0;
  if ( hex_receive_options( pab ) == HEXSTAT_SUCCESS ) {
    len = buffer[ 0 ] + ( buffer[ 1 ] << 8 );
    att = buffer[ 2 ];
  }
  if ( !prn_open ) {
    if ( (res == HEXSTAT_SUCCESS) && ( att != OPENMODE_WRITE ) ) {
      rc = HEXSTAT_OPTION_ERR;
    }
    if ( res != HEXSTAT_SUCCESS ) {
      rc = res;
    }
  } else {
    rc = HEXSTAT_ALREADY_OPEN;
  }
  if (!hex_is_bav()) { // we can send response
    if ( rc == HEXSTAT_SUCCESS )
    {
      prn_open = 1;  // our printer is NOW officially open.
      len = len ? len : sizeof(buffer);
      hex_send_size_response( len );
    }
    else
    {
      hex_send_final_response( rc );
    }
    return HEXERR_SUCCESS;
  }
  hex_finish();
  return HEXERR_BAV;
}


/*
   hex_prn_close() -
   closes printer at device 12 for use from host.
*/
static uint8_t hex_prn_close(__attribute__((unused)) pab_t pab) {
  uint8_t rc = HEXSTAT_SUCCESS;

  if ( !prn_open ) {
    rc = HEXSTAT_NOT_OPEN;
  }
  prn_open = 0;      // mark printer closed regardless.
  if ( !hex_is_bav() ) {
    // send 0000 response with appropriate status code.
    hex_send_final_response( rc );
  }
  return HEXERR_SUCCESS;
}


/*
    hex_prn_write() -
    write data to serial port when printer is open.
*/
static uint8_t hex_prn_write(pab_t pab) {
  uint16_t len;
  uint16_t i;
  UINT     written = 0;
  uint8_t  rc = HEXERR_SUCCESS;

  len = pab.datalen;

  while ( len && rc == HEXERR_SUCCESS ) {
    i = (len >= sizeof(buffer) ? sizeof(buffer) : len);
    rc = hex_getdata(buffer, i);
    timer_check(0);
    /*
        printer open? print a buffer of data.  We hold off
        on continuation until we've actually got the data
        flushed from the serial buffers before we continue.
        Digital logic analyzer shows some "glitchy" behavior
        on our HSK signal that may be due (somehow) to serial
        port use.  Rather than take the chance, ensure that
        serial data is flushed before proceeding to make sure
        that HexBus operations are not compromised.
    */
    if ( rc == HEXSTAT_SUCCESS && prn_open ) {
      Serial.write(buffer, i);
      delayMicroseconds( i*72 );
      /* use 72 us per character sent delay.
         digital logic analyzer confirms that @ 115200 baud, the data is
         flushed over the wire BEFORE we continue HexBus operations.
      */
      written = 1; // indicate we actually wrote some data
    }
    len -= i;
  }

  /* if we've written data and our printer is open, finish the line out with
      a CR/LF.
  */
  if ( written && prn_open ) {
    buffer[0] = 13;
    buffer[1] = 10;
    Serial.write(buffer, 2);
    delayMicroseconds(176);
  }
  /*
     if printer is NOT open, report such status back
  */
  if ( !prn_open ) {
    rc = HEXSTAT_NOT_OPEN;   // if printer is NOT open, report such status
  }

  /*
     send response and finish operation
  */
  if (!hex_is_bav() ) {
    hex_send_final_response( rc );
  } else {
    hex_finish();
  }
  return HEXERR_SUCCESS;
}


static uint8_t hex_prn_reset( __attribute__((unused)) pab_t pab) {
  
  prn_reset();
  // release the bus ignoring any further action on bus. no response sent.
  hex_finish();
  // wait here while bav is low
  while ( !hex_is_bav() ) {
    ;
  }
  return HEXERR_SUCCESS;
}


#endif

#ifdef INCLUDE_PRINTER
/*
 * Command handling registry for device
 */
static const cmd_proc fn_table[] PROGMEM = {
  hex_prn_open,
  hex_prn_close,
  hex_prn_write,
  hex_prn_reset,
  NULL // end of table.
};

static const uint8_t op_table[] PROGMEM = {
  HEXCMD_OPEN,
  HEXCMD_CLOSE,
  HEXCMD_WRITE,
  HEXCMD_RESET_BUS,
  HEXCMD_INVALID_MARKER
};
#endif


void prn_register(registry_t *registry) {
#ifdef INCLUDE_PRINTER
  uint8_t i = registry->num_devices;
  
  registry->num_devices++;
  registry->entry[ i ].device_code_start = PRN_DEV;
  registry->entry[ i ].device_code_end = PRN_DEV+9; // support 10 thru 19 as device codes.
  registry->entry[ i ].operation = (cmd_proc *)&fn_table;
  registry->entry[ i ].command = (uint8_t *)&op_table;
#endif
  return;
}

void prn_reset( void )
{
#ifdef INCLUDE_PRINTER
  prn_open = 0; // make sure our printer is closed.
#endif
  return;
}

void prn_init( void )
{
#ifdef INCLUDE_PRINTER
  prn_open = 0;
#endif
  return;
}

