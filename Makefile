CC = g++
INCLUDE_LIBS = -Ipim_interface -Ipim_interface/sdk_internals_2023.2.0
INCLUDE_THIRD_PARTY = -Ithird_party/exprtk/include -Ithird_party/argparse/include -Ithird_party/json/include -Ithird_party/parlaylib/include -I.
CCFLAGS = -Wall -Wextra -O3 `dpu-pkg-config --cflags --libs dpu` -std=c++17 -march=native -pthread -fconcepts
CCDPU = dpu-upmem-dpurte-clang

.PHONY: all debug clean
all: host dpu upmem_direct_c.o

debug: CCFLAGS += -DXFER_BACK
# debug: CCFLAGS += -g -DXFER_BACK
debug: host dpu dpu_checksum

dpu_checksum:
	$(CCDPU) dpu_checksum.c -o dpu_checksum

host: host.cpp pim_interface
	$(CC) -o host host.cpp $(INCLUDE_LIBS) $(INCLUDE_THIRD_PARTY) $(CCFLAGS)

dpu: dpu.c
	$(CCDPU) -o dpu dpu.c

upmem_direct_c.o: upmem_direct_c.c upmem_direct_c.h
	$(CC) -o $@ -c upmem_direct_c.c $(INCLUDE_LIBS) $(INCLUDE_THIRD_PARTY) $(CCFLAGS)

clean:
	rm -f host dpu upmem_direct_c.o
