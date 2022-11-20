/**
 * Sample program that reads tags for a fixed period of time (500ms)
 * and prints the tags found.
 * @file read.c
 */

#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <tm_reader.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <sqlite3.h>
#ifdef TMR_ENABLE_HF_LF
#include <tmr_utils.h>
#endif /* TMR_ENABLE_HF_LF */

#ifdef BARE_METAL
  #define printf(...) {}
#endif

#ifndef BARE_METAL
/* Enable this to use transportListener */
#ifndef USE_TRANSPORT_LISTENER
#define USE_TRANSPORT_LISTENER 0
#endif

#define PRINT_TAG_METADATA 0
#define numberof(x) (sizeof((x))/sizeof((x)[0]))

#define usage() {errx(1, "Please provide valid reader URL, such as: reader-uri [--ant n] [--pow read_power]\n"\
                         "reader-uri : e.g., 'tmr:///COM1' or 'tmr:///dev/ttyS0/' or 'tmr://readerIP'\n"\
                         "[--ant n] : e.g., '--ant 1'\n"\
                         "[--epc epc] : e.g., '--epc E20063993234ADF11A586EB7'\n"\
                         "[--pow read_power] : e.g, '--pow 2300'\n"\
                         "Example for UHF modules: 'tmr:///com4' or 'tmr:///com4 --ant 1,2' or 'tmr:///com4 --ant 1,2 --pow 2300'\n"\
                         "Example for HF/LF modules: 'tmr:///com4' \n");}

struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

void errx(int exitval, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);

  exit(exitval);
}
#endif /* BARE_METAL */

void checkerr(TMR_Reader* rp, TMR_Status ret, int exitval, const char *msg)
{
#ifndef BARE_METAL
  if (TMR_SUCCESS != ret)
  {
    errx(exitval, "Error %s: %s\n", msg, TMR_strerr(rp, ret));
  }
#endif /* BARE_METAL */
}

#ifdef USE_TRANSPORT_LISTENER
void serialPrinter(bool tx, uint32_t dataLen, const uint8_t data[],
                   uint32_t timeout, void *cookie)
{
  FILE *out = cookie;
  uint32_t i;

  fprintf(out, "%s", tx ? "Sending: " : "Received:");
  for (i = 0; i < dataLen; i++)
  {
    if (i > 0 && (i & 15) == 0)
    {
      fprintf(out, "\n         ");
    }
    fprintf(out, " %02x", data[i]);
  }
  fprintf(out, "\n");
}

void stringPrinter(bool tx,uint32_t dataLen, const uint8_t data[],uint32_t timeout, void *cookie)
{
  FILE *out = cookie;

  fprintf(out, "%s", tx ? "Sending: " : "Received:");
  fprintf(out, "%s\n", data);
}
#endif /* USE_TRANSPORT_LISTENER */

#ifndef BARE_METAL
void parseAntennaList(uint8_t *antenna, uint8_t *antennaCount, char *args)
{
  char *token = NULL;
  char *str = ",";
  uint8_t i = 0x00;
  int scans;

  /* get the first token */
  if (NULL == args)
  {
    fprintf(stdout, "Missing argument\n");
    usage();
  }

  token = strtok(args, str);
  if (NULL == token)
  {
    fprintf(stdout, "Missing argument after %s\n", args);
    usage();
  }

  while(NULL != token)
  {
    scans = sscanf(token, "%"SCNu8, &antenna[i]);
    if (1 != scans)
    {
      fprintf(stdout, "Can't parse '%s' as an 8-bit unsigned integer value\n", token);
      usage();
    }
    i++;
    token = strtok(NULL, str);
  }
  *antennaCount = i;
}
#endif /* BARE_METAL */

int main(int argc, char *argv[])
{
  set_conio_terminal_mode();
  TMR_Reader r, *rp;
  TMR_Status ret;
  TMR_ReadPlan plan;
  TMR_Region region;
#define READPOWER_NULL (-12345)
  int readpower = READPOWER_NULL;
#ifndef BARE_METAL
  uint8_t i;
#endif /* BARE_METAL*/
  uint8_t buffer[20];
  uint8_t *antennaList = NULL;
  uint8_t antennaCount = 0x0;
  TMR_TRD_MetadataFlag metadata = TMR_TRD_METADATA_FLAG_ALL;
  char string[100];
  TMR_String model;

  bool saved1 = false;
  bool saved2 = false;
  
  int pow;
  double FREQ_STEP = 5;
  int POW_STEP = 100;
  int MIN_FREQ = 840000;
  int MAX_FREQ = 928000;
  int MIN_POW = 3150;
  int MAX_POW = 3150;
  int NUM_FREQS;
  int TIME = 100; //Seconds
  TMR_uint32List value;

  TMR_PortValue portvalueList[4];
  TMR_PortValueList portvalue;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  char *err_msg = 0;
  char database [15];
  printf("Enter database file name: ");
  scanf("%s", database);
  int rc = sqlite3_open(database, &db);
  if (rc != SQLITE_OK) {
      fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return 1;
  }
  char *sql = "DROP TABLE IF EXISTS ToP;"
              "CREATE TABLE ToP(epc INT, rssi INT, phase INT, freq INT, pow INT);";
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK ) {
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);        
      sqlite3_close(db);
      return 1;
  } 
  char *epc1 = "E200493F3185AD7126ACF6B5";
  char *epc2 = "E200493F38187C3126BE51F0";
  int rssi;
  int phase;
  int freq;
    
#if USE_TRANSPORT_LISTENER
  TMR_TransportListenerBlock tb;
#endif /* USE_TRANSPORT_LISTENER */
  rp = &r;

#ifndef BARE_METAL
  if (argc < 2)
  {
    fprintf(stdout, "Not enough arguments.  Please provide reader URL.\n");
    usage(); 
  }

  for (i = 2; i < argc; i+=2)
  {
    if(0x00 == strcmp("--ant", argv[i]))
    {
      if (NULL != antennaList)
      {
        fprintf(stdout, "Duplicate argument: --ant specified more than once\n");
        usage();
      }
      parseAntennaList(buffer, &antennaCount, argv[i+1]);
      antennaList = buffer;
    }
    else if (0 == strcmp("--pow", argv[i]))
    {
      long retval;
      char *startptr;
      char *endptr;
      startptr = argv[i+1];
      retval = strtol(startptr, &endptr, 0);
      if (endptr != startptr)
      {
        readpower = retval;
        fprintf(stdout, "Requested read power: %d cdBm\n", readpower);
      }
      else
      {
        fprintf(stdout, "Can't parse read power: %s\n", argv[i+1]);
      }
    }
    else if (0 == strcmp("--epc1", argv[i]))
    {
      epc1 = argv[i+1];
    }
    else if (0 == strcmp("--epc2", argv[i]))
    {
      epc2 = argv[i+1];
    }
    else if (0 == strcmp("--freqstep", argv[i]))
    {
      char *freqptr1 = argv[i+1];
      char *freqptr2;
      FREQ_STEP = (double) strtod(freqptr1, &freqptr2);
    }
    else if (0 == strcmp("--powstep", argv[i]))
    {
      char *powptr1 = argv[i+1];
      char *powptr2;
      POW_STEP = strtol(powptr1, &powptr2, 0);
    }
    else if (0 == strcmp("--minfreq", argv[i]))
    {
      char *powptr1 = argv[i+1];
      char *powptr2;
      MIN_FREQ = strtol(powptr1, &powptr2, 0);
    }
    else if (0 == strcmp("--maxfreq", argv[i]))
    {
      char *powptr1 = argv[i+1];
      char *powptr2;
      MAX_FREQ = strtol(powptr1, &powptr2, 0);
    }
    else if (0 == strcmp("--minpow", argv[i]))
    {
      char *powptr1 = argv[i+1];
      char *powptr2;
      MIN_POW = strtol(powptr1, &powptr2, 0);
    }
    else if (0 == strcmp("--maxpow", argv[i]))
    {
      char *powptr1 = argv[i+1];
      char *powptr2;
      MAX_POW = strtol(powptr1, &powptr2, 0);
    }
    else
    {
      fprintf(stdout, "Argument %s is not recognized\n", argv[i]);
      usage();
    }
  }
  NUM_FREQS = (int) (MAX_FREQ-MIN_FREQ)/FREQ_STEP/1000;
  int freqs[NUM_FREQS];
  ret = TMR_create(rp, argv[1]);
  checkerr(rp, ret, 1, "creating reader");
#else
  ret = TMR_create(rp, "tmr:///com1");

#ifdef TMR_ENABLE_UHF
  buffer[0] = 1;
  antennaList = buffer;
  antennaCount = 0x01;
#endif /* TMR_ENABLE_UHF */
#endif /* BARE_METAL */

#if USE_TRANSPORT_LISTENER
  if (TMR_READER_TYPE_SERIAL == rp->readerType)
  {
    tb.listener = serialPrinter;
  }
  else
  {
    tb.listener = stringPrinter;
  }
  tb.cookie = stdout;

  TMR_addTransportListener(rp, &tb);
#endif /* USE_TRANSPORT_LISTENER */

  ret = TMR_connect(rp);
  checkerr(rp, ret, 1, "connecting reader");

  model.value = string;
  model.max   = sizeof(string);
  TMR_paramGet(rp, TMR_PARAM_VERSION_MODEL, &model);
  checkerr(rp, ret, 1, "Getting version model");

  if (0 != strcmp("M3e", model.value))
  {
    region = TMR_REGION_NONE;
    ret = TMR_paramGet(rp, TMR_PARAM_REGION_ID, &region);
    checkerr(rp, ret, 1, "getting region");
    region = TMR_REGION_NONE;
    if (TMR_REGION_NONE == region)
    {
      TMR_RegionList regions;
      TMR_Region _regionStore[32];
      regions.list = _regionStore;
      regions.max = sizeof(_regionStore)/sizeof(_regionStore[0]);
      regions.len = 0;

      ret = TMR_paramGet(rp, TMR_PARAM_REGION_SUPPORTEDREGIONS, &regions);
      checkerr(rp, ret, __LINE__, "getting supported regions");

      if (regions.len < 1)
      {
        checkerr(rp, TMR_ERROR_INVALID_REGION, __LINE__, "Reader doesn't support any regions");
      }

      region = regions.list[22];  // OPEN REGION = 22
      ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &region);
      checkerr(rp, ret, 1, "setting region");
    }

    if (READPOWER_NULL != readpower)
    {
      int value;

      ret = TMR_paramGet(rp, TMR_PARAM_RADIO_READPOWER, &value);
      checkerr(rp, ret, 1, "getting read power");
      // printf("Old read power = %d dBm\n", value);

      value = readpower;
      ret = TMR_paramSet(rp, TMR_PARAM_RADIO_READPOWER, &value);
      checkerr(rp, ret, 1, "setting read power");
    }

    {
      int value;
      ret = TMR_paramGet(rp, TMR_PARAM_RADIO_READPOWER, &value);
      checkerr(rp, ret, 1, "getting read power");
      // printf("Read power = %d dBm\n", value);
    }

#ifdef TMR_ENABLE_UHF
    /**
     * Checking the software version of the sargas.
     * The antenna detection is supported on sargas from software version of 5.3.x.x.
     * If the Sargas software version is 5.1.x.x then antenna detection is not supported.
     * User has to pass the antenna as arguments.
     */
    {
      ret = isAntDetectEnabled(rp, antennaList);
      if(TMR_ERROR_UNSUPPORTED == ret)
      {
#ifndef BARE_METAL
        fprintf(stdout, "Reader doesn't support antenna detection. Please provide antenna list.\n");
        usage();
#endif
      }
      else
      {
        checkerr(rp, ret, 1, "Getting Antenna Detection Flag Status");
      }
    }
#endif /* TMR_ENABLE_UHF */
  }

#ifdef TMR_ENABLE_LLRP_READER
  if (0 != strcmp("Mercury6", model.value))
#endif /* TMR_ENABLE_LLRP_READER */
  {
	// Set the metadata flags. Protocol is mandatory metadata flag and reader don't allow to disable the same
	// metadata = TMR_TRD_METADATA_FLAG_ANTENNAID | TMR_TRD_METADATA_FLAG_FREQUENCY | TMR_TRD_METADATA_FLAG_PROTOCOL;
	ret = TMR_paramSet(rp, TMR_PARAM_METADATAFLAG, &metadata);
	checkerr(rp, ret, 1, "Setting Metadata Flags");
  }

  /**
  * for antenna configuration we need two parameters
  * 1. antennaCount : specifies the no of antennas should
  *    be included in the read plan, out of the provided antenna list.
  * 2. antennaList  : specifies  a list of antennas for the read plan.
  **/
  // initialize the read plan
  if (0 != strcmp("M3e", model.value))
  {
    ret = TMR_RP_init_simple(&plan, antennaCount, antennaList, TMR_TAG_PROTOCOL_GEN2, 1000);
  }
  else
  {
    ret = TMR_RP_init_simple(&plan, antennaCount, antennaList, TMR_TAG_PROTOCOL_ISO14443A, 1000);
  }
  checkerr(rp, ret, 1, "initializing the  read plan");

  /* Commit read plan */
  ret = TMR_paramSet(rp, TMR_PARAM_READ_PLAN, &plan);
  checkerr(rp, ret, 1, "setting read plan");

  for (int i = 0; i <= NUM_FREQS; i++){
    freqs[i] = MIN_FREQ + (int) i*1000*FREQ_STEP;
    // printf("%d = %d\n",i,freqs[i]);
  }

  while (!kbhit()) {
    for (int i = 0; i <= NUM_FREQS; i++){
      saved1 = false;
      saved2 = false;
      pow = MIN_POW;
      value.max = 1;
      value.len = 1;
      value.list = &freqs[i];
      ret = TMR_paramSet(rp, TMR_PARAM_REGION_HOPTABLE, &value);
      checkerr(rp, ret, 1, "Setting Hoptable");

      /* Get the antenna return loss value, this parameter is not the part of reader stats */
      // portvalue.max = sizeof(portvalueList)/sizeof(portvalueList[0]);
      // portvalue.list = portvalueList;
      // ret = TMR_paramGet(rp, TMR_PARAM_ANTENNA_RETURNLOSS, &portvalue);
      // checkerr(rp, ret, 1, "getting the antenna return loss");
      // printf("Antenna Return Loss\n");
      // for (int k = 0; k < portvalue.len && k < portvalue.max; k++)
      // {
      //   printf("Antenna %d | %d \n", portvalue.list[k].port, portvalue.list[k].value);
      // }

      while (pow <= MAX_POW){
        printf("%d : %d\n", freqs[i], pow);
        ret = TMR_paramSet(rp, TMR_PARAM_RADIO_READPOWER, &pow);
        checkerr(rp, ret, 1, "setting read power");
        ret = TMR_read(rp, 500, NULL);
          
        if (TMR_ERROR_TAG_ID_BUFFER_FULL == ret)
        {
          /* In case of TAG ID Buffer Full, extract the tags present
          * in buffer.
          */
        #ifndef BARE_METAL
          fprintf(stdout, "reading tags:%s\n", TMR_strerr(rp, ret));
        #endif /* BARE_METAL */
        }
        else
        {
          checkerr(rp, ret, 1, "reading tags");
        }

        while (TMR_SUCCESS == TMR_hasMoreTags(rp))
          {
            TMR_TagReadData trd;
            char idStr[128];
          #ifndef BARE_METAL
            char timeStr[128];
          #endif /* BARE_METAL */

            ret = TMR_getNextTag(rp, &trd); 
            checkerr(rp, ret, 1, "fetching tag");

            TMR_bytesToHex(trd.tag.epc, trd.tag.epcByteCount, idStr);

          #ifndef BARE_METAL
          TMR_getTimeStamp(rp, &trd, timeStr);
          // printf("Tag ID: %s ", idStr);

          // Enable PRINT_TAG_METADATA Flags to print Metadata value
          #if PRINT_TAG_METADATA
          {
          uint16_t j = 0;

          printf("\n");
          for (j=0; (1<<j) <= TMR_TRD_METADATA_FLAG_MAX; j++)
          {
            if ((TMR_TRD_MetadataFlag)trd.metadataFlags & (1<<j))
            {
              switch ((TMR_TRD_MetadataFlag)trd.metadataFlags & (1<<j))
              {
                case TMR_TRD_METADATA_FLAG_READCOUNT:
                  printf("Read Count: %d\n", trd.readCount);
                  break;
                case TMR_TRD_METADATA_FLAG_ANTENNAID:
                  printf("Antenna ID: %d\n", trd.antenna);
                  break;
                case TMR_TRD_METADATA_FLAG_TIMESTAMP:
                  printf("Timestamp: %s\n", timeStr);
                  break;
                case TMR_TRD_METADATA_FLAG_PROTOCOL:
                  printf("Protocol: %d\n", trd.tag.protocol);
                  break;
          #ifdef TMR_ENABLE_UHF
                case TMR_TRD_METADATA_FLAG_RSSI:
                  printf("RSSI: %d\n", trd.rssi);
                  break;
                case TMR_TRD_METADATA_FLAG_FREQUENCY:
                  printf("Frequency: %d\n", trd.frequency);
                  break;
                case TMR_TRD_METADATA_FLAG_PHASE:
                  printf("Phase: %d\n", trd.phase);
                  break;
          #endif /* TMR_ENABLE_UHF */
                case TMR_TRD_METADATA_FLAG_DATA:
                {
                  //TODO : Initialize Read Data
                  if (0 < trd.data.len)
                  {
          #ifdef TMR_ENABLE_HF_LF
                    if (0x8000 == trd.data.len)
                    {
                      ret = TMR_translateErrorCode(GETU16AT(trd.data.list, 0));
                      checkerr(rp, ret, 0, "Embedded tagOp failed:");
                    }
                    else
          #endif /* TMR_ENABLE_HF_LF */
                    {
                      char dataStr[255];
                      uint32_t dataLen = trd.data.len;

                      //Convert data len from bits to byte(For M3e only).
                      if (0 == strcmp("M3e", model.value))
                      {
                        dataLen = tm_u8s_per_bits(trd.data.len);
                      }

                      TMR_bytesToHex(trd.data.list, dataLen, dataStr);
                      printf("Data(%d): %s\n", trd.data.len, dataStr);
                    }
                  }
                }
                break;
          #ifdef TMR_ENABLE_UHF
                case TMR_TRD_METADATA_FLAG_GPIO_STATUS:
                {
                  if (rp->readerType == TMR_READER_TYPE_SERIAL)
                  {
                    printf("GPI status:\n");
                    for (i = 0 ; i < trd.gpioCount ; i++)
                    {
                      printf("Pin %d: %s\n", trd.gpio[i].id, trd.gpio[i].bGPIStsTagRdMeta ? "High" : "Low");
                    }
                    printf("GPO status:\n");
                    for (i = 0 ; i < trd.gpioCount ; i++)
                    {
                      printf("Pin %d: %s\n", trd.gpio[i].id, trd.gpio[i].high ? "High" : "Low");
                    }
                  }
                  else
                  {
                    printf("GPI status:\n");
                    for (i = 0 ; i < trd.gpioCount/2 ; i++)
                    {
                      printf("Pin %d: %s\n", trd.gpio[i].id, trd.gpio[i].high ? "High" : "Low");
                    }
                    printf("GPO status:\n");
                    for (i = trd.gpioCount/2 ; i < trd.gpioCount ; i++)
                    {
                      printf("Pin %d: %s\n", trd.gpio[i].id, trd.gpio[i].high ? "High" : "Low");
                    }
                  }
                }
                break;
                if (TMR_TAG_PROTOCOL_GEN2 == trd.tag.protocol)
                {
                  case TMR_TRD_METADATA_FLAG_GEN2_Q:
                    printf("Gen2Q: %d\n", trd.u.gen2.q.u.staticQ.initialQ);
                    break;
                  case TMR_TRD_METADATA_FLAG_GEN2_LF:
                  {
                    printf("Gen2Linkfrequency: ");
                    switch(trd.u.gen2.lf)
                    {
                      case TMR_GEN2_LINKFREQUENCY_250KHZ:
                        printf("250(khz)\n");
                        break;
                      case TMR_GEN2_LINKFREQUENCY_320KHZ:
                        printf("320(khz)\n");
                        break;
                      case TMR_GEN2_LINKFREQUENCY_640KHZ:
                        printf("640(khz)\n"); 
                        break;
                      default:
                        printf("Unknown value(%d)\n",trd.u.gen2.lf);
                        break;
                    }
                    break;
                  }
                  case TMR_TRD_METADATA_FLAG_GEN2_TARGET:
                  {
                    printf("Gen2Target: ");
                    switch(trd.u.gen2.target)
                    {
                      case TMR_GEN2_TARGET_A:
                        printf("A\n");
                        break;
                      case TMR_GEN2_TARGET_B:
                        printf("B\n");
                        break;
                      default:
                        printf("Unknown Value(%d)\n",trd.u.gen2.target);
                        break;
                    }
                    break;
                  }
                }
          #endif /* TMR_ENABLE_UHF */
          #ifdef TMR_ENABLE_HF_LF
                case TMR_TRD_METADATA_FLAG_TAGTYPE:
                {
                  printf("TagType: 0x%08lx\n", trd.tagType);
                  break;
                }
          #endif /* TMR_ENABLE_HF_LF */
                default:
                  break;
                }
              }
            }
          }
          #endif
          // printf ("\n");
          #endif
          // printf("input: %s\n", epc);
          // printf("reading: %s\n", idStr);
          // printf("String compare = %d\n", strncmp(epc, idStr, sizeof(idStr)));
          freq = freqs[i];
          if ((0 == strcmp(epc1, idStr)) && (saved1 == false)){
            saved1 = true;
            rssi = trd.rssi;
            phase = trd.phase;
            printf("%s : %d - %d - %d - %d\n", epc1, rssi, phase, freq, pow);
            if (sqlite3_prepare_v2(db, "INSERT INTO ToP(epc, rssi, phase, freq, pow) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL)) {
                printf("Error executing sql statement\n");
                sqlite3_close(db);
                exit(-1);
            }
            sqlite3_bind_text(stmt, 1, epc1, -1, NULL);
            sqlite3_bind_int (stmt, 2, rssi);
            sqlite3_bind_int (stmt, 3, phase);
            sqlite3_bind_int (stmt, 4, freq);
            sqlite3_bind_int (stmt, 5, pow);    
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
          } 
          if ((0 == strcmp(epc2, idStr)) && (saved2 == false)){
            saved2 = true;
            rssi = trd.rssi;
            phase = trd.phase;
            printf("%s : %d - %d - %d - %d\n", epc2, rssi, phase, freq, pow);
            if (sqlite3_prepare_v2(db, "INSERT INTO ToP(epc, rssi, phase, freq, pow) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL)) {
                printf("Error executing sql statement\n");
                sqlite3_close(db);
                exit(-1);
            }
            sqlite3_bind_text(stmt, 1, epc2, -1, NULL);
            sqlite3_bind_int (stmt, 2, rssi);
            sqlite3_bind_int (stmt, 3, phase);
            sqlite3_bind_int (stmt, 4, freq);
            sqlite3_bind_int (stmt, 5, pow);    
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
          } 
          if ((saved1 == true) && (saved2 == true)){pow = MAX_POW;}
        }
        pow = pow + POW_STEP;
        tmr_sleep(500);
      }
      if ((saved1 == false) && (saved2 == false)){
        freq = freqs[i];
        if (sqlite3_prepare_v2(db, "INSERT INTO ToP(epc, rssi, phase, freq, pow) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL)) {
          printf("Error executing sql statement\n");
          sqlite3_close(db);
          exit(-1);
        }
        sqlite3_bind_text(stmt, 1, epc1, -1, NULL);
        sqlite3_bind_int (stmt, 2, -99);
        sqlite3_bind_int (stmt, 3, 0);
        sqlite3_bind_int (stmt, 4, freq);
        sqlite3_bind_int (stmt, 5, 3200);    
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        if (sqlite3_prepare_v2(db, "INSERT INTO ToP(epc, rssi, phase, freq, pow) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL)) {
          printf("Error executing sql statement\n");
          sqlite3_close(db);
          exit(-1);
        }
      }
      sqlite3_bind_text(stmt, 1, epc2, -1, NULL);
      sqlite3_bind_int (stmt, 2, -99);
      sqlite3_bind_int (stmt, 3, 0);
      sqlite3_bind_int (stmt, 4, freq);
      sqlite3_bind_int (stmt, 5, 3200);    
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
  }
  printf("Stopping...\n");
  (void)getch();
  printf("Closing database\n");
  // sqlite3_finalize(stmt);
  tmr_sleep(500);
  sqlite3_close(db);
  TMR_destroy(rp);
  return 0;
}
