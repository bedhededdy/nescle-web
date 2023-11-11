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
#include "Mapper066.h"

#include "../Cart.h"

namespace NESCLE {
void Mapper066::Reset() {
    bank_select = 0;
}

uint8_t Mapper066::MapCPURead(uint16_t addr) {
    addr %= 0x8000;
    uint8_t select = (bank_select & 0x30) >> 4;
    return cart.ReadPrgRom((size_t)((select << 15) | addr));
}

bool Mapper066::MapCPUWrite(uint16_t addr, uint8_t data) {
    bank_select = data;
    return true;
}

uint8_t Mapper066::MapPPURead(uint16_t addr) {
    uint8_t select = bank_select & 0x03;
    return cart.ReadChrRom((size_t)((select << 13) | addr));
}

bool Mapper066::MapPPUWrite(uint16_t addr, uint8_t data) {
    if (cart.GetChrRomBlocks() == 0) {
        uint8_t select = bank_select & 0x03;
        cart.WriteChrRom((size_t)((select << 13) | addr), data);
        return true;
    }
    return false;
}

void Mapper066::ToJSON(nlohmann::json& json) const {
    Mapper::ToJSON(json);
    json["bank_select"] = bank_select;
}

void Mapper066::FromJSON(const nlohmann::json& json) {
    Mapper::FromJSON(json);
    bank_select = json["bank_select"];
}
}
