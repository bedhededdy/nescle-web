~/emsdk/upstream/emscripten/em++.bat --bind ESEmu.cpp emu-core/mappers/Mapper.cpp emu-core/mappers/Mapper000.cpp emu-core/mappers/Mapper001.cpp emu-core/mappers/Mapper002.cpp emu-core/mappers/Mapper003.cpp emu-core/mappers/Mapper004.cpp emu-core/mappers/Mapper007.cpp emu-core/mappers/Mapper066.cpp emu-core/CPU.cpp emu-core/APU.cpp emu-core/Bus.cpp emu-core/Cart.cpp emu-core/PPU.cpp Util.cpp -O2 -s EXPORT_ES6=1 -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web -s MODULARIZE=1 -s EXPORTED_FUNCTIONS=[_malloc,_free] -IemscriptenIncludes -s ASSERTIONS=1 --embind-emit-tsd a.out.d.ts