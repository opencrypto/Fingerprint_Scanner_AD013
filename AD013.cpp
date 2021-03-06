
// ================================================
// Capacitative Fingerprint Sensor Library         
//   (c) 2020 by Massimiliano Pala and CableLabs   
//   All Rights Reserved                           
//                                                 
// Fingerprint / RFID / BLE Project                
// ================================================

// Local Include
#include "AD013.h"

// Uses this library to harmonize support
// for meaningful output across boards
#include <LibPrintf.h>

// Global Definitions
#define AD013_MSG_HEADER_SIZE     10
#define AD013_MAX_ACK_BUFF_SIZE   20
#define AD013_MAX_BIN_BUFF_SIZE  128

// Message Offsets
#define AD013_MSG_OFFSET_HEADER    0
#define AD013_MSG_OFFSET_DEVID     2
#define AD013_MSG_OFFSET_FLAG      6
#define AD013_MSG_OFFSET_LENGTH    7
#define AD013_MSG_OFFSET_CODE      9
#define AD013_MSG_OFFSET_DATA     10

// Debugging Messaging
#ifdef AD013_DEBUG
#define AD013_DEBUG_IS_ENABLED     1
#else
#define AD013_DEBUG_IS_ENABLED     0
#endif

typedef enum {
  AD013_CODE_OK                     = 0x00,
  AD013_CODE_ERROR                  = 0x01,
  AD013_CODE_NO_FINGER              = 0x02,
  AD013_CODE_IMAGE_FAIL             = 0x03,
  AD013_CODE_FEATURE_FAIL_LIGTH_DRY = 0x04,
  AD013_CODE_FEATURE_FAIL_DARK_WET  = 0x05,
  AD013_CODE_FEATURE_FAIL_AMORPHOUS = 0x06,
  AD013_CODE_FEATURE_FAIL_MINUTIAE  = 0x07,
  AD013_CODE_FINGER_NOT_MATCHED     = 0x08,
  AD013_CODE_FINGER_NOT_FOUND       = 0x09,
  AD013_CODE_FEATURE_FAIL_MERGE     = 0x0A,
  AD013_CODE_TEMLATE_DB_RANGE_ERROR = 0x0B,
  AD013_CODE_TEMPLATE_READ_ERROR    = 0x0C,
  AD013_CODE_FEATURE_UPLOAD_FAIL    = 0x0D,
  AD013_CODE_DATA_RECEIVE_ERROR     = 0x0E,
  AD013_CODE_DATA_IMAGE_UPLOAD_FAIL = 0x0F,
  AD013_CODE_DELETE_FAIL            = 0x10,
  AD013_CODE_TEMPLATE_DB_CLEAR_FAIL = 0x11,
  AD013_CODE_LOW_POWER_MODE_ERROR   = 0x12,
  AD013_CODE_PASSWORD_ERROR         = 0x13,
  AD013_CODE_RESET_FAIL             = 0x14,
  AD013_CODE_IMAGE_INCOMPLETE_ERROR = 0x15,
  AD013_CODE_ONLINE_UPGRADE_FAIL    = 0x16,
  AD013_CODE_IMAGE_STILL_DATA_ERROR = 0x17,
  AD013_CODE_FLASH_READ_WRITE_ERROR = 0x18,
  AD013_CODE_GENERIC_ERROR          = 0x19,
  AD013_CODE_DATA_RECEIVED_OK       = 0xF0, /* Ack with 0xF0 after receiving data correctly */
  AD013_CODE_DATA_CONTINUE_ACK      = 0xF1,
  AD013_CODE_FLASH_SUM_ERROR        = 0xF2,
  AD013_CODE_FLASH_FLAG_ERROR       = 0xF3,
  AD013_CODE_FLASH_PKT_LENGTH_ERROR = 0xF4,
  AD013_CODE_FLASH_CODE_TOO_LONG    = 0xF5,
  AD013_CODE_FLASH_ERROR            = 0xF6,
  AD013_CODE_REGISTER_NUMBER_ERROR  = 0x1A,
  AD013_CODE_REGISTER_WRONG_DISTRO_NUMBER = 0x1B,
  AD013_CODE_NOTEPAD_PAGE_NUMBER_ERROR = 0x1C,
  AD013_CODE_PORT_OP_FAIL           = 0x1D,
  AD013_CODE_AUTO_ENROLL_FAIL       = 0x1E,
  AD013_CODE_TEMPLATE_DB_FULL       = 0x1F
  /* 0x20 - 0xEF Reserved Values */
} AD013_CODE;

                        // ================
                        // Global Variables
                        // ================

// Defaults
char AD013_def_passwd[4] = { 0x00 };
char AD013_def_devid[4] = { 0xFF };

// Global Variables
AD013_Params AD013_DefaultParams = {
  { 0xFF, 0xFF, 0xFF, 0xFF }, // Default DeviceID
  { 0x00 },  // Zero-Padded Empty Params
  0          // Param Length (Zero is Empty)
};

// Global Variable(s)
static const char msgTemplate[10] = {
  0xEF, 0x01,             /* Header */
  0xFF, 0xFF, 0xFF, 0xFF, /* Device ID */
  0x01,                   /* Flag */
  0x00, 0x03,             /* Data Length (Code/Data + Sum) */
  0x00                    /* Code/Data */
  /* Sum (char[2]) Omitted from the Template */
};


                        // =============================
                        // Internal Functions Prototypes
                        // =============================

uint16_t AD013_get_uint16_value(char * val);
void AD013_set_uint16_value(char * buff, uint16_t val);

int AD013_AddParam1(AD013_Params * params, uint8_t val);
int AD013_AddParam2(AD013_Params * params, uint16_t val);
int AD013_AddParamN(AD013_Params * params, char * buff, uint8_t size);

int AD013_Send (int           code,
              Stream     &  SensorCom,
              AD013_Params *  params             = NULL,
              byte       ** recv_data_buff     = NULL,
              int        *  recv_data_buff_len = NULL);
              
int AD013_Recv(char * data, int data_len);

#define AD013_ClearParams(a) \
  (a)->size = 0

#define PS_VerifyPwd(a,b) \
  AD013_Send(0x13,a,b)

#define PS_GetImage(a) \
  AD013_Send(0x01,a)

#define PS_GenChar(a,b) \
  AD013_Send(0x02,a,b)

#define PS_Search(a,b,c,d) \
  AD013_Send(0x04,a,b,c,d)

                        // =============================
                        // Fingerprint Utility Functions
                        // =============================

uint16_t AD013_get_uint16_value(char * val) {
  
  uint16_t ret = 0;
  byte * pnt = (byte *) &ret;

  *pnt = (byte) *((byte *)val + 1);
  *(pnt + 1) = (byte) *((byte *)val);

  return ret;
}

void AD013_set_uint16_value(char * buff, uint16_t val) {
  
  byte * pnt1 = (byte *) &val;
  byte * pnt2 = pnt1 + 1;
  
  if (!buff) return;

  *((byte *)buff) = (byte) *pnt2;
  *((byte *)(buff + 1)) = (byte) *pnt1;
}


int AD013_AddParam1(AD013_Params * params, uint8_t val) {
  if (!params || params->size > AD013_MAX_PARAMS_SIZE - 1)
    return -1;

  // Fixes the value of the size
  if (params->size < 0) params->size = 0;

  // Stores the uint16_t param by reverting the
  // order of the bytes
  params->buff[params->size++] = val;
  return params->size;
}

int AD013_AddParam2(AD013_Params * params, uint16_t val) {
  char * pnt = (char *)&val;
  if (!params || params->size > AD013_MAX_PARAMS_SIZE - 2)
    return -1;

  // Fixes the value of the size
  if (params->size < 0) params->size = 0;

  // Stores the uint16_t param by reverting the
  // order of the bytes
  params->buff[params->size++] = *(pnt + 1);
  params->buff[params->size++] = *(pnt);

  return params->size;
}

int AD013_AddParamN(AD013_Params * params, char * buff, uint8_t size) {
  char * pnt = buff;
  if (!params || !buff || params->size > AD013_MAX_PARAMS_SIZE - size)
    return -1;

   memcpy(params->buff + params->size, buff, size);
   params->size += size;
      
   return params->size;
}

int AD013_Send (int           code,
              Stream     &  Sensorcom, 
              AD013_Params *  params,
              byte       ** recv_data_buff,
              int        *  recv_data_buff_len) {

  // Receive Buffer
  char     recv_buff[20] = { 0x00 };
  uint16_t recv_buff_len = 0;
  int      recv_code     = -1;

  int read_chars = 0;
  int i = 0;
  
  uint16_t len = 0;
  uint16_t sum = 0;
  uint16_t max_retries = 5;
  
  // Small Checks
  if ((params != NULL) && (params->buff == NULL || params->size < 1))
    return -1;

  // Send Buffer Size
  int send_buff_len = 12 + (params != NULL ? params->size : 0);

  // Allocates the needed Memory
  char * send_buff = (char *) malloc(send_buff_len);

  // Sets the Defaults
  memcpy(send_buff, msgTemplate, sizeof(msgTemplate));

  // Sets the right message code
  send_buff[AD013_MSG_OFFSET_CODE] = (uint8_t) code;

  if ((params != NULL) && (params->size > 0)) {
    memcpy(send_buff + AD013_MSG_OFFSET_DATA, params->buff, params->size);
  }

  // Sets the Packet Length [Code (1) + Sum (2) + params_len (Var)]
  len = 3 + (params != NULL ? params->size : 0);
  byte *pnt1 = (byte *) &len;
  byte *pnt2 = pnt1 + 1;

  AD013_set_uint16_value(send_buff + AD013_MSG_OFFSET_LENGTH, len);

  // Calculates the Checksum
  for (i = AD013_MSG_OFFSET_FLAG; i < send_buff_len - 2; i++) {
    // Calculates the Sum
    sum = (sum + (uint8_t) send_buff[i]) & 65535;
  }

  // Saves the Sum
  AD013_set_uint16_value(&send_buff[AD013_MSG_OFFSET_DATA + (params != NULL ? params->size : 0)], sum);

  // printf("SENT SUM: %02X\n", 
  //   AD013_get_uint16_value(send_buff + send_buff_len - 2););
  
  // Writes the Fixed header
  SensorCom.write((byte *)send_buff, send_buff_len);

  // Now we need to read the ACK packet. First we get the
  // fixed size of the packet;
  while (recv_buff_len < AD013_MSG_HEADER_SIZE + 2) {
    if (0 >= --max_retries) break;
    // while (SensorCom.available() == false)
    //  delay(1); //Wait for user input
    read_chars = SensorCom.readBytes(recv_buff + recv_buff_len, sizeof(recv_buff) - recv_buff_len);
    recv_buff_len += read_chars; 
  }

  if (recv_buff_len < 10) {
    printf("ERROR: Cannot Read (Timeout Reached; Read: %d bytes Reply)\n", recv_buff_len);
    goto err;
  }

  // Let's check the message is ok
  if (memcmp(send_buff, recv_buff, 5) == 0) {
    
    uint16_t pkt_len = 0;
    uint16_t recv_sum = 0;
    uint16_t sum = 0;
    uint16_t max_data = 0;

    // Gets the Packet (Anything After Length) Data Size
    pkt_len = AD013_get_uint16_value(recv_buff + AD013_MSG_OFFSET_LENGTH);

    // Gets the Checksum from the Received Message
    recv_sum = AD013_get_uint16_value(recv_buff + recv_buff_len - 2);

    // Gets the Code from the received message
    recv_code = (uint8_t) *(recv_buff + AD013_MSG_OFFSET_CODE);
    
    // Calculates the Sum
    for (i = AD013_MSG_OFFSET_FLAG ; i < recv_buff_len - 2; i++) {
      sum = (sum + recv_buff[i]) & 65535;
    }

    // Compares the Checksums, if an error, let's reject
    // the message and return the error
    if (sum != recv_sum) {
      printf("CHECKSUM ERROR: Received = %02X, Calculated = %02\n", 
        recv_sum, sum);
      if (send_buff) free(send_buff);
      return -99;
    }

    // Here we should get the parameters
    if (recv_data_buff) {
      // If the pointer is provided
      if (*recv_data_buff == NULL) {
        // If the Buffer is not Provided, we allocate it
        max_data = pkt_len - 2;
        *recv_data_buff = (byte *) malloc (pkt_len - 2);
      } else {
        // Gets the size of the input buffer (if provided)
        max_data = *recv_data_buff_len;
      }
      // We have a good buffer, now let's fill it in
      memcpy(*recv_data_buff, &recv_buff[AD013_MSG_OFFSET_DATA], max_data);
    }
    
  } else {
    if (AD013_DEBUG_IS_ENABLED) printf("ERROR: Received checksum does not match.\n");
    goto err;
  }
  
  if (send_buff) free(send_buff);
  
  return recv_code;

err:

  // Debug Information
  printf("MSG SENT: ");
  for (i = 0 ; i < send_buff_len; i++) {
    printf("%02.2X:", send_buff[i]);
  }
  Serial.println();
  delay(50);
  
  printf("MSG RECV: ");
  for (i = 0; i < recv_buff_len; i++) {
    printf("%02.2X:", recv_buff[i]);
  }
  Serial.println();
  delay(50);

  // Free Allocated Memory
  if (send_buff) free(send_buff);

  // Error
  return -1;
}

int AD013_Recv(char * data, int data_len) {

  char buff[128];
  
  int read_chars = 0;
  int i = 0;
  
  read_chars = SensorCom.readBytes(buff, sizeof(buff));

  return 1;
}

                        // ================================
                        // Fingerprint High-Level Functions
                        // ================================

int CFP_FindSensor(Stream     & SensorCom,
                   int          serSpeed,
                   AD013_Params * params) {
  // Let's Check we have a sensor attached and we can
  // verify the password. Use the params to modify the
  // defaults
  
  AD013_Params myParams;
    // Container for params

  SoftwareSerial * swSerial = (SoftwareSerial *) &SensorCom;
    // Container for a more generic Serial interface
    
  if (!params) {
    myParams = AD013_DefaultParams;
    // Adds the Password as the default command
    AD013_AddParamN(&myParams, AD013_def_passwd, sizeof(AD013_def_passwd));
  } else {
    // Copies the params struct into the local variable
    myParams = *params;
  }

  // Sets the Default Timeout
  SensorCom.setTimeout(defTimeout);

  if (serSpeed < 0) {
    // Array Of Speeds To Try
    int speedVals[5] = {115200, 57600, 38400, 19200, 9600};
    // Debug Info
    if (AD013_DEBUG_IS_ENABLED)
      printf("Looking for Fingerprint Sensor - checking 115200-9600 baud range\n");
 
    // Check which Speed Works
    for (int i = 0; i < sizeof(speedVals)/sizeof(int); i++) {
      if (AD013_DEBUG_IS_ENABLED) printf("Checking Speed %d baud ....: ", speedVals[i]);
      // SensorCom.begin(speedVals[i]);
      swSerial->begin(speedVals[i]);
      delay(100);
      if (AD013_Send(0x13, SensorCom, &myParams) < 0) {
        if (AD013_DEBUG_IS_ENABLED) printf("Not Supported\n");
      } else {
        if (AD013_DEBUG_IS_ENABLED) printf("Ok (Supported).\n");
        return 1;
      }
    }
    
    // Debug
    if (AD013_DEBUG_IS_ENABLED)
      printf("All Speed Failed, Aborting.\n");

    // ALL speeds fail, let's fail
    return -1;
    
  } else {

    // If Speed was requested, let's set the speed
    if (serSpeed > 0) swSerial->begin(serSpeed);
    delay(50);
  
    // Execute the call
    if (AD013_Send(0x13, SensorCom, &myParams) < 0) return -1;
  }
  
  // All Done
  return 1;
}

int AD013_SearchTemplate (Stream & SensorCom,
                        int      timeOut,
                        int      threashold,
                        bool     SecurityOfficerOnly) {

  int defDelayPeriod = 240; // Built-in Delay for processing
  int delayPeriod = 120;
  int code = -1;

  // Allocates the params and initializes w/ default values
  AD013_Params params = AD013_DefaultParams;

  // Debug Information
  if (AD013_DEBUG_IS_ENABLED)
    printf("Please put finger on sensor...\n");

  // Generates the Char/Template to Serch for
  while ((code = PS_GetImage(SensorCom)) != 0) {

    // Checks for specific errors
    if (code == 0x01 || code == 0x03) {
      // Packet Error (0x01) or Failure (0x03)
      if (AD013_DEBUG_IS_ENABLED)
        printf("ERROR: Cannot Get Image (code: %d)\n", code);
    }

    // Code 0x02 is for Fingerprint NOT on sensor (polling)
    
    // Checks for Timeout Conditions
    if (timeOut <= 0) {
      if (AD013_DEBUG_IS_ENABLED) printf("Timeout Reached, aborting...\n");
      break;
    } else {
      delay(delayPeriod);
      timeOut -= delayPeriod + defDelayPeriod;
    }
  }

  // If we do not have a successful finger on
  // the sensor, let's abort
  if (code != 0) return (-1);

  // DEBUG information
  if (AD013_DEBUG_IS_ENABLED)
    printf("Preparing to Match Finger...\n");

  // Adds the number of the buffer to match (1)
  AD013_AddParam1(&params, 1);

  // Generates the Char/Template from the acquired
  // Image. Possible error codes are handled in the
  // switch statement
  code = PS_GenChar(SensorCom, &params);

  // Error Handling
  switch (code) {
    case AD013_CODE_ERROR                  /* 0x01 */ :
    case AD013_CODE_FEATURE_FAIL_AMORPHOUS /* 0x06 */ :
    case AD013_CODE_FEATURE_FAIL_MINUTIAE  /* 0x07 */ :
    case AD013_CODE_IMAGE_INCOMPLETE_ERROR /* 0x15 */ : {
      if (AD013_DEBUG_IS_ENABLED) 
        printf("DETECTED CFS ERROR [%d]\n", code);
      return -1;
    } break;

    default:
      if (AD013_DEBUG_IS_ENABLED) printf("CFS CODE: %d\n", code);
  }

  // Builds the new params
  AD013_ClearParams(&params);  // Clears the Parameters
  AD013_AddParam1(&params, 1); // Adds Buffer Num. Param (1 byte)
  AD013_AddParam2(&params, 0); // Adds Start Num. Param (2 bytes)
  AD013_AddParam2(&params, 99);// Adds End Num. Param (2 bytes)
  
  // Search the DB for the Generated Char
  // byte data[20] = { 0x00 };
  byte * data = NULL;
  int len = 0;
  int matched_template = PS_Search(SensorCom, &params, (byte **) &data, &len);

  if (matched_template >= 0) {
    if (AD013_DEBUG_IS_ENABLED)
      printf("Matched Template: %d (Template: %d, Score: %d)\n",
        matched_template, AD013_get_uint16_value((char *)data), 
        AD013_get_uint16_value((char *)&data[2]));
  } else {
    printf("ERROR: Code %d\n");
  }

  if (data) free(data);

  return 1;
}

/* !\brief Clears one template from the fingerprint DB */

int AD013_ClearTemplates (Stream & SerialPort,
					    int      rangeStart,
					    int      rangeEnd) {
  // DEBUG: PLACEHOLDER: Missing Code
  printf("AD013_ClearTemplate() Not Implemented, yet.\n");
  return -1;
}

                      
/* !\brief Clears all user templates from the fingerprint DB */

int AD013_ClearUserTemplates (Stream & SerialPort) {
  // DEBUG: PLACEHOLDER: Missing Code
  printf("AD013_ClearUserTemplates() Not Implemented, yet.\n");
  return -1;
}


/* !\brief Clears all the Security Officer (SO) templates from the
           fingerprint DB */

int AD013_ClearSOTemplates(Stream * SerialPort) {
  {
  // DEBUG: PLACEHOLDER: Missing Code
  printf("AD013_ClearSOTemplates() Not Implemented, yet.\n");
  return -1;
}
}

/* !\brief Enrolls a new Finger into the Sensor's DB */

int AD013_Enroll(Stream & SerialPort, bool isSecurityOfficer) {

	// DEBUG: PLACEHOLDER: MISSING CODE
	if (AD013_DEBUG_IS_ENABLED) printf("AD013_Enroll() Not Implemented, yet.\n");
	return -1;
}
