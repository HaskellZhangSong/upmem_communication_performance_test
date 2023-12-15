// #include "direct_interface.hpp"
#include "upmem_interface.hpp"
#include <iostream>

#define cast_to_cpp(name) reinterpret_cast<UPMEMInterface*>(name)
extern "C" {
  #include <upmem_direct_c.h>
  #include <stdint.h>
  XDirectPIMInterface* newDirectPIMInterface() {
    return reinterpret_cast<XDirectPIMInterface*>(new UPMEMInterface());
  }
  void deleteDirectPIMInterface(XDirectPIMInterface* self) {
    delete (cast_to_cpp(self));
  }
//  void SendToRankMRAM(XDirectPIMInterface* self, uint8_t **buffers, uint32_t symbol_offset, uint8_t *ptr_dest, uint32_t length, int id) {
//    cast_to_cpp(self)->SendToRankMRAM(buffers, symbol_offset, ptr_dest, length, id);
//  }
  void allocate(XDirectPIMInterface* self, uint32_t nr_of_ranks, char* binary) {
    std::string binary2 = std::string(binary);
    cast_to_cpp(self)->allocate(nr_of_ranks, binary2);
  }
  void deallocate(XDPI self){
    cast_to_cpp(self)->deallocate();
  }
  void Launch(XDPI self, bool async) {
    cast_to_cpp(self)->Launch(async);
  }
  void upmem_sync(XDPI self) {
    cast_to_cpp(self)->sync();
  }
  void PrintLog(XDPI self) {
    cast_to_cpp(self)->PrintLog(); 
  }
  void SendToPIM(XDPI self, uint8_t** buffers, char* symbol_name, uint32_t symbol_offset, uint32_t length, bool async) {
    std::string symbol_name2 = std::string(symbol_name);
    cast_to_cpp(self)->SendToPIM(buffers, symbol_name2, symbol_offset, length, async);
  }
  
  void ReceiveFromPIM(XDPI self, uint8_t** buffers, char* symbol_name, uint32_t symbol_offset, uint32_t length, bool async) {
    std::string symbol_name2 = std::string(symbol_name);
    cast_to_cpp(self)->ReceiveFromPIM(buffers, symbol_name2, symbol_offset, length, async);

  }

}
