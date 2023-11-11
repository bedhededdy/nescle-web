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
#ifndef MAPPER001_H_
#define MAPPER001_H_

#include <array>

#include "Mapper.h"

namespace NESCLE {
class Mapper001 : public Mapper {
private:
    uint8_t chr_select4_lo = 0;
    uint8_t chr_select4_hi = 0;
    uint8_t chr_select8 = 0;

    uint8_t prg_select16_lo = 0;
    uint8_t prg_select16_hi = 0;
    uint8_t prg_select32 = 0;

    uint8_t load = 0;
    // Write count
    uint8_t load_reg_ct = 0;
    uint8_t ctrl = 0;

    // TODO: NEED TO ADD THIS
    // int cycles_since_last_write = 0;

    // Since we may not know what the size of the prg ram is from
    // the iNES header, we must allocate the maximum possible amount of 32kb
    std::array<uint8_t, 0x8000> sram;

protected:
    void ToJSON(nlohmann::json& json) const override;
    void FromJSON(const nlohmann::json& json) override;

public:
    Mapper001(uint8_t id, Cart& cart, Mapper::MirrorMode mirror)
        : Mapper(id, cart, mirror) {}

    void Reset() override;

    uint8_t MapCPURead(uint16_t addr) override;
    bool MapCPUWrite(uint16_t addr, uint8_t data) override;
    uint8_t MapPPURead(uint16_t addr) override;
    bool MapPPUWrite(uint16_t addr, uint8_t data) override;
};
}
#endif // MAPPER001_H_
