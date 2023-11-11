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
#include "Mapper007.h"

#include "../Cart.h"

namespace NESCLE {
void Mapper007::Reset() {
    bank_select = 0;
}

uint8_t Mapper007::MapCPURead(uint16_t addr) {
    addr %= 0x8000;
    // use bottom 3 bits for bank selecrt
    uint8_t select = bank_select & 0x07;
    return cart.ReadPrgRom((size_t)((select << 15) | addr));
}

bool Mapper007::MapCPUWrite(uint16_t addr, uint8_t data) {
    bank_select = data;
    mirror_mode = (data & 0x10) ? Mapper::MirrorMode::ONESCREEN_HI :
        Mapper::MirrorMode::ONESCREEN_LO;
    return true;
}

uint8_t Mapper007::MapPPURead(uint16_t addr) {
    return cart.ReadChrRom(addr);
}

bool Mapper007::MapPPUWrite(uint16_t addr, uint8_t data) {
    if (cart.GetChrRomBlocks() == 0) {
        cart.WriteChrRom(addr, data);
        return true;
    }
    return false;
}

void Mapper007::ToJSON(nlohmann::json& json) const {
    Mapper::ToJSON(json);
    json["bank_select"] = bank_select;
}

void Mapper007::FromJSON(const nlohmann::json& json) {
    Mapper::FromJSON(json);
    bank_select = json["bank_select"];
}
}
