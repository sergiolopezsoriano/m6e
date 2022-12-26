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
                         "[--pow read_power] : e.g, '-pow 3150'\n"\
                         "[--time reading_time] : e.g, '--time 10 (seconds)'\n"\
                         "[--file file_name] : e.g, '--file database.db'\n"\
                         "[--reg region] : e.g, '--reg 1 (Europe) 2 (USA)'\n"\
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

time_t getSeconds(struct TMR_Reader *rp, const struct TMR_TagReadData *read)
{
    uint8_t shift;
    uint64_t timestamp;
    time_t seconds;
    shift = 32;
    timestamp = ((uint64_t)read->timestampHigh<<shift) | read->timestampLow;
    seconds = timestamp / 1000;
    return seconds;
}

int main(int argc, char *argv[])
{
  // set_conio_terminal_mode();
  TMR_Reader r, *rp;
  TMR_Status ret;
  TMR_ReadPlan plan;
  TMR_Region region;
#define READPOWER_NULL (-12345)
  int readpower = 3000; // READPOWER_NULL
#ifndef BARE_METAL
  uint8_t i;
#endif /* BARE_METAL*/
  uint8_t buffer[20];
  uint8_t *antennaList = NULL;
  uint8_t antennaCount = 0x0;
  TMR_TRD_MetadataFlag metadata = TMR_TRD_METADATA_FLAG_ALL;
  char string[100];
  TMR_String model;

  time_t time2;
  time_t time1;
  time ( &time1 );
  double delta = 5;
  int ts = 0;

  int reg = 1;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  char *err_msg = 0;
  char *database = "default.db";
  // printf("Enter database file name: ");
  // scanf("%s", database);
  TMR_uint32List value;
    
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
    else if (0 == strcmp("--time", argv[i]))
    {
      long retval;
      char *startptr;
      char *endptr;
      startptr = argv[i+1];
      retval = strtol(startptr, &endptr, 0);
      if (endptr != startptr)
      {
        delta = retval;
        fprintf(stdout, "Reading time: %f s\n", delta);
      }
      else
      {
        fprintf(stdout, "Can't parse reading time: %s\n", argv[i+1]);
      }
    }
    else if (0 == strcmp("--reg", argv[i]))
    {
      long retval;
      char *startptr;
      char *endptr;
      startptr = argv[i+1];
      retval = strtol(startptr, &endptr, 0);
      if (endptr != startptr)
      {
        reg = retval;
        fprintf(stdout, "Region: %f s\n", delta);
      }
      else
      {
        fprintf(stdout, "Can't parse region: %s\n", argv[i+1]);
      }
    }
    else if (0 == strcmp("--file", argv[i]))
    {
      
      database = argv[i+1];
    }
    else
    {
      fprintf(stdout, "Argument %s is not recognized\n", argv[i]);
      usage();
    }
  }
  int rc = sqlite3_open(database, &db);
  if (rc != SQLITE_OK) {
      fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return 1;
  }
  char *sql = "DROP TABLE IF EXISTS ToP;"
              "CREATE TABLE ToP(epc INT, rssi INT, phase INT, freq INT, pow INT, ant INT, ts INT, read_count INT, protocol INT);";
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK ) {
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);        
      sqlite3_close(db);
      return 1;
  } 
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

      region = regions.list[reg];  // OPEN REGION = 22
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

  while (difftime(time2,time1) < delta && !kbhit()) {
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
      #endif
      ts = getSeconds(rp, &trd);
      printf("%s | %d | %d | %d | %d | %d | %d\n", idStr, readpower, trd.rssi, trd.phase, trd.frequency, trd.antenna, ts);
      if (sqlite3_prepare_v2(db, "INSERT INTO ToP(epc, rssi, phase, freq, pow, ant, ts, read_count, protocol) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL)) {
          printf("Error executing sql statement\n");
          sqlite3_close(db);
          exit(-1);
      }
      sqlite3_bind_text(stmt, 1, idStr, -1, NULL);
      sqlite3_bind_int (stmt, 2, trd.rssi);
      sqlite3_bind_int (stmt, 3, trd.phase);
      sqlite3_bind_int (stmt, 4, trd.frequency);
      sqlite3_bind_int (stmt, 5, readpower);    
      sqlite3_bind_int (stmt, 6, trd.antenna);
      sqlite3_bind_int (stmt, 7, ts);
      sqlite3_bind_int (stmt, 8, trd.readCount);
      sqlite3_bind_int (stmt, 9, trd.tag.protocol); 
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    time ( &time2 );
  }
  printf("Stopping...\n");
  printf("Closing database\n");
  // sqlite3_finalize(stmt);
  tmr_sleep(500);
  sqlite3_close(db);
  TMR_destroy(rp);
  return 0;
}
