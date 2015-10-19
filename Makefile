CC=gcc -std=gnu99
CXX=g++
CFLAGS=-c -O3 -I . -I libwebm -I libyuv/include -g -D_FORTIFY_SOURCE=2
LDFLAGS=
LIBS=-pthread -lvpx -lm -lc -ludt
SOURCES= args.c y4minput.c ivfdec.c ivfenc.c rate_hist.c tools_common.c warnings.c vpxstats.c vpxenc.c libyuv/source/cpu_id.cc libyuv/source/planar_functions.cc libyuv/source/row_any.cc libyuv/source/row_common.cc libyuv/source/row_gcc.cc libyuv/source/row_mips.cc libyuv/source/row_neon.cc libyuv/source/row_neon64.cc libyuv/source/row_win.cc libyuv/source/scale.cc libyuv/source/scale_any.cc libyuv/source/scale_common.cc libyuv/source/scale_gcc.cc libyuv/source/scale_mips.cc libyuv/source/scale_neon.cc libyuv/source/scale_neon64.cc libyuv/source/scale_win.cc webmenc.cc webm_chunk_writer.cc webm_live_muxer.cc libwebm/mkvmuxer.cpp libwebm/mkvmuxerutil.cpp libwebm/mkvwriter.cpp streamer.cpp
OBJECTS=$(SOURCES:%.c=%.o)
OBJECTS_FULL=$(OBJECTS:%.cc=%.o)

all: live_encoder

live_encoder: $(OBJECTS_FULL)
	$(CXX) $(LDFLAGS) $(OBJECTS_FULL) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

.cc.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	find . -name \*.o | xargs rm
	rm live_encoder
