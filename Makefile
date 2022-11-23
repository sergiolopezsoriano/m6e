LIB = /home/sergi/ws/m6e/c/src/api/libmercuryapi.a
SQL1 = /usr/lib/aarch64-linux-gnu/libsqlite3.a
SQL2 = /snap/lxd/22761/lib/libsqlite3.so

# $(addprefix prefix,names…)
API=/home/sergi/ws/m6e/c/src/api/
HEADERS += $(API)serial_reader_imp.h
HEADERS += $(API)tm_config.h
HEADERS += $(API)tm_reader.h
HEADERS += $(API)tmr_filter.h
HEADERS += $(API)tmr_gen2.h
HEADERS += $(API)tmr_gpio.h
HEADERS += $(API)tmr_ipx.h
HEADERS += $(API)tmr_iso180006b.h
HEADERS += $(API)tmr_iso14443a.h
HEADERS += $(API)tmr_iso14443b.h
HEADERS += $(API)tmr_iso15693.h
HEADERS += $(API)tmr_lf125khz.h
HEADERS += $(API)tmr_lf134khz.h
HEADERS += $(API)tmr_params.h
HEADERS += $(API)tmr_read_plan.h
HEADERS += $(API)tmr_region.h
HEADERS += $(API)tmr_serial_reader.h
HEADERS += $(API)tmr_serial_transport.h
HEADERS += $(API)tmr_status.h
HEADERS += $(API)tmr_tag_auth.h
HEADERS += $(API)tmr_tag_data.h
HEADERS += $(API)tmr_tag_lock_action.h
HEADERS += $(API)tmr_tag_protocol.h
HEADERS += $(API)tmr_tagop.h
HEADERS += $(API)tmr_types.h
HEADERS += $(API)tmr_utils.h

DBG ?= -g
CWARN = -Werror
CFLAGS += -D TMR_ENABLE_SERIAL_READER_ONLY=1
CFLAGS += -I$(API) $(DBG) $(CWARN) -I/usr/include/
CFLAGS += -fPIC

CODE = /home/sergi/ws/m6e/c/src/m6e/
PROG4 := power_ramp
PROG2 := read
PROG3 := power_ramp2
PROG1 := continuous_readings
PROGS += $(PROG1)
PROGS += $(PROG2)
PROGS += $(PROG3)
PROGS += $(PROG4)

# TERMINAL
# all: $(PROGS)
# power_ramp.o: $(HEADERS) $(LIB)
# power_ramp: power_ramp.o $(LIB)
# 	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# power_ramp: power_ramp.o $(LIB)
# 	$(CC) $(CFLAGS) -o power_ramp power_ramp.o /home/sergi/ws/m6e/c/src/api/libmercuryapi.a -lpthread
# power_ramp.o: $(HEADERS) $(LIB)
# 	$(CC) $(CFLAGS) -c -o power_ramp.o power_ramp.c

# VSCODE continuous_readings.c
$(CODE)$(PROG1): $(CODE)$(PROG1).o $(LIB) $(SQL1) $(SQL2)
	$(CC) $(CFLAGS) -o $(CODE)$(PROG1) $(CODE)$(PROG1).o /snap/lxd/22761/lib/libsqlite3.so /usr/lib/aarch64-linux-gnu/libsqlite3.a /home/sergi/ws/m6e/c/src/api/libmercuryapi.a -lpthread
$(CODE)$(PROG1).o: $(HEADERS) $(LIB) $(SQL1) $(SQL2)
	$(CC) $(CFLAGS) -c -o $(CODE)$(PROG1).o $(CODE)$(PROG1).c

# VSCODE power_ramp
# /home/sergi/ws/m6e/c/src/m6e/power_ramp: /home/sergi/ws/m6e/c/src/m6e/power_ramp.o $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -o /home/sergi/ws/m6e/c/src/m6e/power_ramp /home/sergi/ws/m6e/c/src/m6e/power_ramp.o /snap/lxd/22761/lib/libsqlite3.so /usr/lib/aarch64-linux-gnu/libsqlite3.a /home/sergi/ws/m6e/c/src/api/libmercuryapi.a -lpthread
# /home/sergi/ws/m6e/c/src/m6e/power_ramp.o: $(HEADERS) $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -c -o /home/sergi/ws/m6e/c/src/m6e/power_ramp.o /home/sergi/ws/m6e/c/src/m6e/power_ramp.c

# VSCODE power_ramp2
# /home/sergi/ws/m6e/c/src/m6e/power_ramp2: /home/sergi/ws/m6e/c/src/m6e/power_ramp2.o $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -o /home/sergi/ws/m6e/c/src/m6e/power_ramp2 /home/sergi/ws/m6e/c/src/m6e/power_ramp2.o /snap/lxd/22761/lib/libsqlite3.so /usr/lib/aarch64-linux-gnu/libsqlite3.a /home/sergi/ws/m6e/c/src/api/libmercuryapi.a -lpthread
# /home/sergi/ws/m6e/c/src/m6e/power_ramp2.o: $(HEADERS) $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -c -o /home/sergi/ws/m6e/c/src/m6e/power_ramp2.o /home/sergi/ws/m6e/c/src/m6e/power_ramp2.c

# $(CODE)$(PROGS): $(CODE)$(PROGS).o $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -o $(CODE)$(PROGS) $(CODE)$(PROGS).o $(SQL1) $(SQL2) $(LIB) -lpthread
# $(CODE)$(PROGS).o: $(HEADERS) $(LIB) $(SQL1) $(SQL2)
# 	$(CC) $(CFLAGS) -c -o $(CODE)$(PROGS).o $(CODE)$(PROGS).c

.PHONY: clean
clean:
	rm -f $(PROGS) *.o