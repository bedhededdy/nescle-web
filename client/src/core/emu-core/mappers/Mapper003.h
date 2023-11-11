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
#ifndef MAPPER003_H_
#define MAPPER003_H_

#include "Mapper.h"

namespace NESCLE {
class Mapper003 : public Mapper {
private:
    uint8_t bank_select;

public:
    Mapper003(uint8_t id, Cart& cart, Mapper::MirrorMode mirror)
        : Mapper(id, cart, mirror), bank_select(0) {}

    void Reset() override;

    uint8_t MapCPURead(uint16_t addr) override;
    bool MapCPUWrite(uint16_t addr, uint8_t data) override;
    uint8_t MapPPURead(uint16_t addr) override;
    bool MapPPUWrite(uint16_t addr, uint8_t data) override;

protected:
    void ToJSON(nlohmann::json& json) const override;
    void FromJSON(const nlohmann::json& json) override;
};
}
#endif // MAPPER003_H_
