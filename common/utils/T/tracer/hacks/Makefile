CC=gcc
CFLAGS=-Wall -g -pthread -DT_TRACER -I. -I..

LIBS=-lX11 -lm -lpng -lXft

all: dump_nack_signal time_meas timeplot multi-rru-clean ant0 plot-ofdm

dump_nack_signal: ../utils.o ../database.o ../configuration.o ../event.o \
                  dump_nack_signal.o
	$(CC) $(CFLAGS) -o dump_nack_signal $^ $(LIBS)

time_meas: ../utils.o ../database.o ../configuration.o ../event.o \
                  time_meas.o
	$(CC) $(CFLAGS) -o time_meas $^ $(LIBS)

timplot: timeplot.o
	$(CC) $(CFLAGS) -o timeplot $^ $(LIBS)

multi-rru-clean: ../utils.o ../database.o ../event.o ../configuration.o multi-rru-clean.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

ant0: ant0.o ../utils.o ../database.o ../configuration.o ../event.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

#ant0 needs good compilation, conversion to float is heavy
ant0.o: ant0.c
	$(CC) $(CFLAGS) -c -o $@ $< -march=native -ffast-math -O3

plot-ofdm: plot-ofdm.o ../utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -lfftw3f

#plot-ofdm needs good compilation
plot-ofdm.o: plot-ofdm.c
	$(CC) $(CFLAGS) -c -o $@ $< -march=native -ffast-math -O3

.PHONY: all

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o core dump_nack_signal time_meas timeplot multi-rru-clean \
              ant0 plot-ofdm
