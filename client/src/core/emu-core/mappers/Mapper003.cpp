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
#include "Mapper003.h"

#include "../Cart.h"

namespace NESCLE {
void Mapper003::Reset() {
    bank_select = 0;
}

uint8_t Mapper003::MapCPURead(uint16_t addr) {
    addr %= cart.GetPrgRomBlocks() > 1 ? 0x8000 : 0x4000;
    return cart.ReadPrgRom(addr);
}

bool Mapper003::MapCPUWrite(uint16_t addr, uint8_t data) {
    bank_select = data;
    return true;
}

uint8_t Mapper003::MapPPURead(uint16_t addr) {
    // Only care abt bottom 2 bits
    uint8_t select = bank_select & 0x03;

    // Without selection, we can only address from 0 to 0x1fff, which is 13
    // bits. Therefore to determine the bank, we must examine the 14th and
    // 15th bits
    return cart.ReadChrRom((size_t)((select << 13) | addr));
}

bool Mapper003::MapPPUWrite(uint16_t addr, uint8_t data) {
    // TODO: LEAVE THIS ALONE FOR NOW, BUT I PROBABLY NEED TO ACCT FOR
    // RAM HERE
    Util_Log(Util_LogLevel::WARN, Util_LogCategory::APPLICATION,
             "Mapper003::MapPPUWrite() attempt to write to chr rom");
    return false;
}

void Mapper003::ToJSON(nlohmann::json& json) const {
    Mapper::ToJSON(json);
    json["bank_select"] = bank_select;
}

void Mapper003::FromJSON(const nlohmann::json& json) {
    Mapper::FromJSON(json);
    bank_select = json["bank_select"];
}
}
