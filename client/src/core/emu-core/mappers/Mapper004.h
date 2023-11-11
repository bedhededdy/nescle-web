/*
 * Copyright 2023 Edward C. Pinkston
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MAPPER004_H_
#define MAPPER004_H_

#include <array>

#include "Mapper.h"

namespace NESCLE {
class Mapper004 : public Mapper {
private:
    // FIXME: SHOULDN'T THESE BE 8 BYTES
    // FIXME: CONVERT THESE TO std::arrays
    uint32_t registers[8];
    uint32_t chr_banks[8];
    uint32_t prg_banks[4];

    uint8_t target_register = 0;
    bool prg_bank_mode = false;
    bool chr_inversion = false;

    bool irq_active = false;
    bool irq_enabled = false;
    bool irq_update = false;
    uint16_t irq_counter;
    uint16_t irq_reload;

    std::array<uint8_t, 0x8000> sram;

public:
    Mapper004(uint8_t id, Cart& cart, Mapper::MirrorMode mirror)
        : Mapper(id, cart, mirror) {}

    void Reset() override;

    uint8_t MapCPURead(uint16_t addr) override;
    bool MapCPUWrite(uint16_t addr, uint8_t data) override;
    uint8_t MapPPURead(uint16_t addr) override;
    bool MapPPUWrite(uint16_t addr, uint8_t data) override;

    void CountdownScanline() override;
    bool GetIRQStatus() override;
    void ClearIRQStatus() override;

protected:
    void ToJSON(nlohmann::json& json) const override;
    void FromJSON(const nlohmann::json& json) override;
};
}
#endif // MAPPER004_H_
