#pragma once

#include "pim_interface.hpp"
#include <iostream>
#include <immintrin.h>
#include "parlay/parallel.h"
#include "parlay/internal/sequence_ops.h"
#include "timer.hpp"

extern "C" {
#include <dpu_management.h>
#include <dpu_program.h>
#include <dpu_rank.h>
#include <dpu_target_macros.h>
#include <dpu_types.h>
#include <dpu_internals.h>

#include "dpu_region_address_translation.h"
#include "hw_dpu_sysfs.h"
#include "sdk_internal_functions.hpp"

typedef struct _fpga_allocation_parameters_t {
    bool activate_ila;
    bool activate_filtering_ila;
    bool activate_mram_bypass;
    bool activate_mram_refresh_emulation;
    unsigned int mram_refresh_emulation_period;
    char *report_path;
    bool cycle_accurate;
} fpga_allocation_parameters_t;

typedef struct _hw_dpu_rank_allocation_parameters_t {
    struct dpu_rank_fs rank_fs;
    struct dpu_region_address_translation translate;
    uint64_t region_size;
    uint8_t mode, dpu_chip_id, backend_id;
    uint8_t channel_id;
    uint8_t *ptr_region;
    bool bypass_module_compatibility;
    /* Backends specific */
    fpga_allocation_parameters_t fpga;
} *hw_dpu_rank_allocation_parameters_t;

dpu_error_t ci_poll_rank(struct dpu_rank_t *rank, dpu_bitfield_t *run,
			 dpu_bitfield_t *fault);

void polling_thread_set_dpu_running(uint32_t idx);

dpu_error_t dpu_sync_rank(struct dpu_rank_t *rank);

#define DPU_REGION_MODE_UNDEFINED 0
#define DPU_REGION_MODE_PERF 1
#define DPU_REGION_MODE_SAFE 2
#define DPU_REGION_MODE_HYBRID 3

// dpu_error_t dpu_switch_mux_for_rank(struct dpu_rank_t *rank,
				    // bool set_mux_for_host);

}

class DirectPIMInterface : public PIMInterface {
   public:
    internal_timer t;

    DirectPIMInterface() : PIMInterface() {
        ranks = nullptr;
        params = nullptr;
        base_addrs = nullptr;
        program = nullptr;
    }

    inline bool aligned(uint64_t offset, uint64_t factor) {
        return (offset % factor == 0);
    }

    // not modifying Launch currently because the default "error handling" seems to be useful.
    void Launch(bool async) {
        auto async_parameter = async ? DPU_ASYNCHRONOUS : DPU_SYNCHRONOUS;
        DPU_ASSERT(dpu_launch(dpu_set, async_parameter));
    }
    // void Launch(bool async) {
    //     assert(!async);
    //     // dpu_error_t status = DPU_OK;
    //     printf("boot\n");
    //     fflush(stdout);
    //     dpu_boot_rank(ranks[0]);
    //     double t = internal_timer::get_timestamp();
    //     dpu_bitfield_t dpu_poll_running[DPU_MAX_NR_CIS];
    //     dpu_bitfield_t dpu_poll_in_fault[DPU_MAX_NR_CIS];
    //     while (true) {
    //         usleep(10000);
    //         ci_poll_rank(ranks[0], dpu_poll_running, dpu_poll_in_fault);
    //         uint64_t* r1 = (uint64_t*)dpu_poll_running;
    //         uint64_t* r2 = (uint64_t*)dpu_poll_in_fault;
    //         printf("%llx\t%llx\n", *r1, *r2);
    //         fflush(stdout);
    //         double t2 = internal_timer::get_timestamp();
    //         if (t2 - t > 1.0) {
    //             return;
    //         }
    //     }
    //     polling_thread_set_dpu_running(ranks[0]->api.thread_info.queue_idx);
    //     dpu_sync_rank(ranks[0]);
    // }

    // inline uint64_t get_correct_offset_fast(uint64_t address_offset, uint32_t dpu_id) {
    //     uint64_t mask_move_7 = (~((1 << 22) - 1)) + (1 << 13); // 31..22, 13
    //     uint64_t mask_move_6 = ((1 << 22) - (1 << 15)); // 21..15
    //     uint64_t mask_move_14 = (1 << 14); // 14
    //     uint64_t mask_move_4 = (1 << 13) - 1; // 12 .. 0
    //     return ((address_offset & mask_move_7) << 7) | ((address_offset & mask_move_6) << 6) | ((address_offset & mask_move_14) << 14) | ((address_offset & mask_move_4) << 4) | (dpu_id << 18);
    // }

    // inline uint64_t get_correct_offset_golden(uint64_t address_offset, uint32_t dpu_id) {
    //     // uint64_t fastoffset = get_correct_offset_fast(address_offset, dpu_id);
    //     uint64_t offset = 0;
    //     // 1 : address_offset < 64MB
    //     offset += (512ll << 20) * (address_offset >> 22);
    //     address_offset &= (1ll << 22) - 1;
    //     // 2 : address_offset < 4MB
    //     if (address_offset & (16 << 10)) {
    //         offset += (256ll << 20);
    //     }
    //     offset += (2ll << 20) * (address_offset / (32 << 10));
    //     address_offset %= (16 << 10);
    //     // 3 : address_offset < 16K
    //     if (address_offset & (8 << 10)) {
    //         offset += (1ll << 20);
    //     }
    //     address_offset %= (8 << 10);
    //     offset += address_offset * 16;
    //     // 4 : address_offset < 8K
    //     offset += (dpu_id & 3) * (256 << 10);
    //     // 5
    //     if (dpu_id >= 4) {
    //         offset += 64;
    //     }
    //     return offset;
    // }

    inline uint64_t GetCorrectOffsetMRAM(uint64_t address_offset, uint32_t dpu_id) {
        auto FastPath = [](uint64_t address_offset, uint32_t dpu_id) {
            uint64_t mask_move_7 = (~((1 << 22) - 1)) + (1 << 13); // 31..22, 13
            uint64_t mask_move_6 = ((1 << 22) - (1 << 15)); // 21..15
            uint64_t mask_move_14 = (1 << 14); // 14
            uint64_t mask_move_4 = (1 << 13) - 1; // 12 .. 0
            return ((address_offset & mask_move_7) << 7) | ((address_offset & mask_move_6) << 6) | ((address_offset & mask_move_14) << 14) | ((address_offset & mask_move_4) << 4) | (dpu_id << 18);
        };

        auto OraclePath = [](uint64_t address_offset, uint32_t dpu_id) {
            // uint64_t fastoffset = get_correct_offset_fast(address_offset, dpu_id);
            uint64_t offset = 0;

            // 1 : address_offset < 64MB
            offset += (512ll << 20) * (address_offset >> 22);
            address_offset &= (1ll << 22) - 1;

            // 2 : address_offset < 4MB
            if (address_offset & (16 << 10)) {
                offset += (256ll << 20);
            }
            offset += (2ll << 20) * (address_offset / (32 << 10));
            address_offset %= (16 << 10);

            // 3 : address_offset < 16K
            if (address_offset & (8 << 10)) {
                offset += (1ll << 20);
            }
            address_offset %= (8 << 10);
            
            offset += address_offset * 16;

            // 4 : address_offset < 8K
            offset += (dpu_id & 3) * (256 << 10);

            // 5
            if (dpu_id >= 4) {
                offset += 64;
            }

            return offset;
        };

        // uint64_t v1 = FastPath(address_offset, dpu_id);
        // uint64_t v2 = OraclePath(address_offset, dpu_id);
        // assert(v1 == v2);
        // return v1;

        return FastPath(address_offset, dpu_id);
    }

    //buffers[64][length]
    void ReceiveFromRankMRAM(uint8_t **buffers, uint32_t symbol_offset,
                         uint8_t *ptr_dest, uint32_t length) {
        assert(aligned(symbol_offset, sizeof(uint64_t)));
        assert(aligned(length, sizeof(uint64_t)));
        assert((uint64_t)symbol_offset + length <= MRAM_SIZE);

        uint64_t cache_line[8], cache_line_interleave[8];

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                // 8 shards of DPUs
                uint64_t offset = GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);
                __builtin_ia32_clflushopt((void *)(ptr_dest + offset));
                offset += 0x40;
                __builtin_ia32_clflushopt((void *)(ptr_dest + offset));
            }
        }
        __builtin_ia32_mfence();

        auto LoadData = [](uint64_t* cache_line, uint8_t* ptr_dest) {
            // printf("%016llx - %016llx\n", ptr_dest + offset, ptr_dest + offset + 0x40);
            // memcpy(cache_line, ptr_dest, sizeof(uint64_t) * 8);
            cache_line[0] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        0 * sizeof(uint64_t)));
            cache_line[1] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        1 * sizeof(uint64_t)));
            cache_line[2] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        2 * sizeof(uint64_t)));
            cache_line[3] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        3 * sizeof(uint64_t)));
            cache_line[4] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        4 * sizeof(uint64_t)));
            cache_line[5] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        5 * sizeof(uint64_t)));
            cache_line[6] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        6 * sizeof(uint64_t)));
            cache_line[7] =
                *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                        7 * sizeof(uint64_t)));
        };

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                if ((i % 8 == 0) && (i + 8 < length / sizeof(uint64_t))) {
                    for (int j = 0; j < 16; j ++) {
                        __builtin_prefetch(((uint64_t *)buffers[j * 4 + dpu_id]) + i + 8);
                    }
                }
                uint64_t offset = GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);
                __builtin_prefetch(ptr_dest + offset + 0x40 * 6);
                __builtin_prefetch(ptr_dest + offset + 0x40 * 7);

                LoadData(cache_line, ptr_dest + offset);
                byte_interleave_avx2(cache_line, cache_line_interleave);
                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id] == nullptr) {
                        continue;
                    }
                    *(((uint64_t *)buffers[j * 8 + dpu_id]) + i) = cache_line_interleave[j];
                }

                offset += 0x40;
                LoadData(cache_line, ptr_dest + offset);
                byte_interleave_avx2(cache_line, cache_line_interleave);
                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id + 4] == nullptr) {
                        continue;
                    }
                    *(((uint64_t *)buffers[j * 8 + dpu_id + 4]) + i) = cache_line_interleave[j];
                }
            }
        }
    }

    //buffers[64][length]
    void SendToRankMRAM(uint8_t **buffers, uint32_t symbol_offset,
                         uint8_t *ptr_dest, uint32_t length, int id) {
        assert(aligned(symbol_offset, sizeof(uint64_t)));
        assert(aligned(length, sizeof(uint64_t)));
        assert((uint64_t)symbol_offset + length <= MRAM_SIZE);

        uint64_t cache_line[8];

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                if ((i % 8 == 0) && (i + 8 < length / sizeof(uint64_t))) {
                    for (int j = 0; j < 16; j ++) {
                        __builtin_prefetch(((uint64_t *)buffers[j * 4 + dpu_id]) + i + 8);
                    }
                }
                uint64_t offset = GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);

                auto ValidAddress = [&](uint8_t* address, int id) {
                    bool ok = true;
                    // if (!(address >= base_addrs[id] + (512ull << 20))) {
                    //     ok = false;
                    // }
                    if (!((address + 128) <= base_addrs[id] + (8ull << 30))) {
                        ok = false;
                    }
                    return ok;
                };

                if (!ValidAddress(ptr_dest + offset, id)) {
                    printf("symbol_offset=%16llx\n", symbol_offset);
                    printf("ptr_dest=%016llx\n", ptr_dest);
                    printf("base_addr=%016llx\n", base_addrs[id]);
                    printf("offset=%d\n", offset);
                    printf("id=%d\n", id);
                    assert(false);
                }

                for (int j = 0; j < 8; j ++) {
                    if (buffers[j * 8 + dpu_id] == nullptr) {
                        continue;
                    }
                    cache_line[j] = *(((uint64_t *)buffers[j * 8 + dpu_id]) + i);
                }
                byte_interleave_avx512(cache_line, (uint64_t*)(ptr_dest + offset), true);

                offset += 0x40;
                for (int j = 0; j < 8; j ++) {
                    if (buffers[j * 8 + dpu_id + 4] == nullptr) {
                        continue;
                    }
                    cache_line[j] = *(((uint64_t *)buffers[j * 8 + dpu_id + 4]) + i);
                }
                byte_interleave_avx512(cache_line, (uint64_t*)(ptr_dest + offset), true);
            }
        }

        __builtin_ia32_mfence();
    }

    bool DirectAvailable(uint8_t **buffers, std::string symbol_name,
                        uint32_t symbol_offset, uint32_t length,
                        bool async_transfer) {
        (void)buffers;
        (void)symbol_offset;
        (void)length;

        // Only suport synchronous transfer
        if (async_transfer) {
            return false;
        }

        for (uint32_t i = 0; i < nr_of_ranks; i ++) {
            if (params[i]->mode != DPU_REGION_MODE_PERF) {
                return false;
            }
        }

        return true;
    }

    // Find symbol address offset
    uint32_t GetSymbolOffset(std::string symbol_name) {
        dpu_symbol_t symbol;
        DPU_ASSERT(
            dpu_get_symbol(program, symbol_name.c_str(), &symbol));
        return symbol.address;
    }

    void ReceiveFromRankWRAM(uint8_t **buffers, uint32_t wram_word_offset,
                             uint32_t nb_of_words, dpu_rank_t *rank) {
        // LOG_RANK(DEBUG, rank, "%p, %u, %u", transfer_matrix, wram_word_offset, nb_of_words);
        if (nb_of_words == 0) {
            return;
        }

        auto verify = [](uint32_t wram_word_offset, uint32_t nb_of_words, dpu_rank_t *rank) {
            verify_wram_access(wram_word_offset, nb_of_words, rank);
            return DPU_OK;
        };

        if (!(verify(wram_word_offset, nb_of_words, rank) == DPU_OK)) {
            printf("ERROR: invalid wram access ((%d >= %d) || (%d > %d))",                                                          \
                wram_word_offset,                                                                                                               \
                (rank)->description->hw.memories.wram_size,                                                                         \
                (wram_word_offset) + (nb_of_words),                                                                                                       \
                (rank)->description->hw.memories.wram_size);
                fflush(stdout);
            assert(false);
        }

        uint8_t nr_cis =
            rank->description->hw.topology.nr_of_control_interfaces;
        uint8_t nr_dpus_per_ci =
            rank->description->hw.topology.nr_of_dpus_per_control_interface;
        dpu_error_t status;

        dpuword_t *wram_array[DPU_MAX_NR_CIS] = { 0 };
        dpu_member_id_t each_dpu;

        for (each_dpu = 0; each_dpu < nr_dpus_per_ci; ++each_dpu) {
            dpu_slice_id_t each_ci;
            uint8_t mask = 0;

            for (each_ci = 0; each_ci < nr_cis; ++each_ci) {
                dpuword_t *dst =
                    (dpuword_t *)
                        buffers[get_transfer_matrix_index(
                            rank, each_dpu, each_ci)]; // reversed here

                if (dst != NULL) {
                    wram_array[each_ci] = dst;
                    mask |= CI_MASK_ONE(each_ci);
                }
            }

            if (mask != 0) {
                FF(ufi_select_dpu(rank, &mask, each_dpu));
                FF(ufi_wram_read(rank, mask, wram_array,
                        wram_word_offset, nb_of_words));
            }
        }
        return;
end:
        std::cout<<"ReceiveFromRankWRAM ERROR"<<std::endl;
        exit(0);
}

    void ReceiveFromWRAM(uint8_t **buffers, uint32_t symbol_base_offset,
                        uint32_t symbol_offset, uint32_t length,
                        bool async_transfer) {
        symbol_offset += symbol_base_offset;
        // printf("Heap Pointer Offset: %lx\n", GetSymbolOffset(DPU_MRAM_HEAP_POINTER_NAME));
        // printf("Symbol Offset: %lx\n", symbol_offset);
        uint32_t wram_word_offset = symbol_offset >> 2;
        uint32_t nb_of_words = length >> 2;

        for (size_t i = 0; i < nr_of_ranks; i ++) {
            ReceiveFromRankWRAM(&buffers[i * MAX_NR_DPUS_PER_RANK],
                                wram_word_offset, nb_of_words, ranks[i]);
        }
        // parlay::parallel_for(
        //     0, nr_of_ranks,
        //     [&](size_t i) {
        //         ReceiveFromRankWRAM(&buffers[i * MAX_NR_DPUS_PER_RANK],
        //                             wram_word_offset, nb_of_words, ranks[i]);
        //     },
        //     1, false);
    }

    void ReceiveFromMRAM(uint8_t **buffers, uint32_t symbol_base_offset,
                        uint32_t symbol_offset, uint32_t length,
                        bool async_transfer) {
        assert(symbol_base_offset & MRAM_ADDRESS_SPACE);
        symbol_offset += symbol_base_offset ^ MRAM_ADDRESS_SPACE;
        auto send = [&](size_t i) {
            DPU_ASSERT(dpu_switch_mux_for_rank_expr(ranks[i], true));
            ReceiveFromRankMRAM(&buffers[i * MAX_NR_DPUS_PER_RANK],
                                symbol_offset, base_addrs[i], length);
        };

        for (size_t i = 0; i < nr_of_ranks; i ++) {
            send(i);
        }
        // parlay::parallel_for(0, nr_of_ranks, send, 1, false);

        // unused. too slow for unknown reason.
        // auto fine_grained_work_stealing = [&]() {
        //     size_t total_work = nr_of_ranks * length;
        //     size_t block_size = 204800 / MAX_NR_DPUS_PER_RANK;
        //     parlay::internal::sliced_for(
        //         total_work, block_size,
        //         [&](size_t idx, size_t start, size_t end) {
        //             assert(start % 8 == 0);
        //             assert(end % 8 == 0);
        //             size_t rank_id_start = start / length;
        //             size_t rank_id_end = (end - 1) / length;
        //             // First rank
        //             {
        //                 size_t rank_id = rank_id_start;
        //                 size_t offset = start % length;
        //                 if (offset != 0) {
        //                     uint8_t *buffers_local[MAX_NR_DPUS_PER_RANK];
        //                     for (int i = 0; i < MAX_NR_DPUS_PER_RANK; i++) {
        //                         uint8_t *target =
        //                             buffers_alligned[rank_id *
        //                                                  MAX_NR_DPUS_PER_RANK
        //                                                  +
        //                                              i];
        //                         if (target == nullptr) {
        //                             buffers_local[i] = nullptr;
        //                         } else {
        //                             buffers_local[i] = target + offset;
        //                         }
        //                     }
        //                     ReceiveFromRankMRAM(
        //                         buffers_local, symbol_offset + offset,
        //                         base_addrs[rank_id], length - offset);
        //                 } else {
        //                     ReceiveFromRankMRAM(
        //                         &buffers_alligned[rank_id *
        //                                           MAX_NR_DPUS_PER_RANK],
        //                         symbol_offset, base_addrs[rank_id], length);
        //                 }
        //             }
        //             // Middle ranks
        //             for (size_t rank_id = rank_id_start + 1;
        //                  rank_id < rank_id_end; rank_id++) {
        //                 ReceiveFromRankMRAM(
        //                     &buffers_alligned[rank_id *
        //                     MAX_NR_DPUS_PER_RANK], symbol_offset,
        //                     base_addrs[rank_id], length);
        //             }
        //             // Last rank
        //             if (rank_id_end > rank_id_start) {
        //                 size_t rank_id = rank_id_end;
        //                 size_t last_rank_len = end % length;
        //                 ReceiveFromRankMRAM(
        //                     &buffers_alligned[rank_id *
        //                     MAX_NR_DPUS_PER_RANK], symbol_offset,
        //                     base_addrs[rank_id], last_rank_len);
        //             }
        //         });
        // };
    }

    void ReceiveFromPIM(uint8_t **buffers, std::string symbol_name,
                        uint32_t symbol_offset, uint32_t length,
                        bool async_transfer) {
        // Please make sure buffers don't overflow
        assert(DirectAvailable(buffers, symbol_name, symbol_offset, length,
                               async_transfer));

        uint32_t symbol_base_offset = GetSymbolOffset(symbol_name);

        // Skip disabled PIM modules
        uint8_t *buffers_alligned[MAX_NR_RANKS * MAX_NR_DPUS_PER_RANK];
        {
            uint32_t offset = 0;
            for (uint32_t i = 0; i < nr_of_ranks; i++) {
                for (int j = 0; j < MAX_NR_DPUS_PER_RANK; j++) {
                    if (ranks[i]->dpus[j].enabled) {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            buffers[offset++];
                    } else {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            nullptr;
                    }
                }
            }
            assert(offset == nr_of_dpus);
        }

        if (symbol_base_offset & MRAM_ADDRESS_SPACE) {  // receive from mram
            // Only support heap pointer at present
            assert(symbol_name == DPU_MRAM_HEAP_POINTER_NAME);
            ReceiveFromMRAM(buffers_alligned, symbol_base_offset, symbol_offset,
                            length, async_transfer);
        } else {  // receive from wram
            ReceiveFromWRAM(buffers_alligned, symbol_base_offset, symbol_offset,
                            length, async_transfer);
        }
    }

    void SendToPIM(uint8_t **buffers, std::string symbol_name,
                   uint32_t symbol_offset, uint32_t length,
                   bool async_transfer) {
        // Please make sure buffers don't overflow
        assert(DirectAvailable(buffers, symbol_name, symbol_offset, length,
                               async_transfer));

        assert(GetSymbolOffset(symbol_name) & MRAM_ADDRESS_SPACE);
        symbol_offset += GetSymbolOffset(symbol_name) ^ MRAM_ADDRESS_SPACE;

        // Skip disabled PIM modules
        uint8_t *buffers_alligned[MAX_NR_RANKS * MAX_NR_DPUS_PER_RANK];
        {
            uint32_t offset = 0;
            for (uint32_t i = 0; i < nr_of_ranks; i++) {
                for (int j = 0; j < MAX_NR_DPUS_PER_RANK; j++) {
                    if (ranks[i]->dpus[j].enabled) {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            buffers[offset++];
                    } else {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            nullptr;
                    }
                }
            }
            assert(offset == nr_of_dpus);
        }

        parlay::parallel_for(
            0, nr_of_ranks,
            [&](size_t i) {
                DPU_ASSERT(dpu_switch_mux_for_rank_expr(ranks[i], true));
                SendToRankMRAM(&buffers_alligned[i * MAX_NR_DPUS_PER_RANK],
                               symbol_offset, base_addrs[i], length, i);
            },
            1, false);

        // Find physical address for each rank
        // for (uint32_t i = 0; i < nr_of_ranks; i++) {
        //     dpu_lock_rank(ranks[i]);
        //     DPU_ASSERT(dpu_switch_mux_for_rank_expr(ranks[i], true)); // 2us
        //     SendToRankMRAM(&buffers[i * DPU_PER_RANK], symbol_offset,
        //     base_addrs[i],
        //                length, i);
        //     dpu_unlock_rank(ranks[i]);
        // }
    }

    void allocate(uint32_t nr_of_ranks, std::string binary) {
        PIMInterface::allocate(nr_of_ranks, binary);
        nr_of_ranks = this->nr_of_ranks;

        ranks = new dpu_rank_t *[nr_of_ranks];
        params = new hw_dpu_rank_allocation_parameters_t[nr_of_ranks];
        base_addrs = new uint8_t *[nr_of_ranks];
        for (uint32_t i = 0; i < nr_of_ranks; i++) {
            ranks[i] = dpu_set.list.ranks[i];
            params[i] =
                ((hw_dpu_rank_allocation_parameters_t)(ranks[i]
                                                           ->description
                                                           ->_internals.data));
            base_addrs[i] = params[i]->ptr_region;
        }

        // find program pointer
        DPU_FOREACH(dpu_set, dpu, each_dpu) {
            assert(dpu.kind == DPU_SET_DPU);
            dpu_t *dpuptr = dpu.dpu;
            if (!dpu_is_enabled(dpuptr)) {
                continue;
            }
            dpu_program_t *this_program = dpu_get_program(dpuptr);
            if (program == nullptr) {
                program = this_program;
            }
            assert(program == nullptr || program == this_program);
        }
    }

    void deallocate() {
        if (ranks != nullptr) {
            delete[] ranks;
        }
        if (base_addrs != nullptr) {
            delete[] base_addrs;
        }
    }

    ~DirectPIMInterface() { deallocate(); }

    //    private:
    void byte_interleave_avx2(uint64_t *input, uint64_t *output) {
        __m256i tm = _mm256_set_epi8(
            15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0,

            15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
        char *src1 = (char *)input, *dst1 = (char *)output;

        __m256i vindex = _mm256_setr_epi32(0, 8, 16, 24, 32, 40, 48, 56);
        __m256i perm = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

        __m256i load0 = _mm256_i32gather_epi32((int *)src1, vindex, 1);
        __m256i load1 = _mm256_i32gather_epi32((int *)(src1 + 4), vindex, 1);

        __m256i transpose0 = _mm256_shuffle_epi8(load0, tm);
        __m256i transpose1 = _mm256_shuffle_epi8(load1, tm);

        __m256i final0 = _mm256_permutevar8x32_epi32(transpose0, perm);
        __m256i final1 = _mm256_permutevar8x32_epi32(transpose1, perm);

        _mm256_storeu_si256((__m256i *)&dst1[0], final0);
        _mm256_storeu_si256((__m256i *)&dst1[32], final1);
    }

    void byte_interleave_avx512(uint64_t *input, uint64_t *output,
                                bool use_stream) {
        __m512i mask;

        mask = _mm512_set_epi64(0x0f0b07030e0a0602ULL, 0x0d0905010c080400ULL,

                                0x0f0b07030e0a0602ULL, 0x0d0905010c080400ULL,

                                0x0f0b07030e0a0602ULL, 0x0d0905010c080400ULL,

                                0x0f0b07030e0a0602ULL, 0x0d0905010c080400ULL);

        __m512i vindex = _mm512_setr_epi32(0, 8, 16, 24, 32, 40, 48, 56, 4, 12,
                                           20, 28, 36, 44, 52, 60);
        __m512i perm = _mm512_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13,
                                         10, 14, 11, 15);

        __m512i load = _mm512_i32gather_epi32(vindex, input, 1);
        __m512i transpose = _mm512_shuffle_epi8(load, mask);
        __m512i final = _mm512_permutexvar_epi32(perm, transpose);

        if (use_stream) {
            _mm512_stream_si512((__m512i *)output, final);
            return;
        }

        _mm512_storeu_si512((__m512i *)output, final);
    }

    const int MRAM_ADDRESS_SPACE = 0x8000000;
    dpu_rank_t **ranks;
    hw_dpu_rank_allocation_parameters_t *params;
    uint8_t **base_addrs;
    dpu_program_t *program;
    // map<std::string, uint32_t> offset_list;
};
