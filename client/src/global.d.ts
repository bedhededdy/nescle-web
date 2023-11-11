/// <reference types="./core/a.out.d.ts" />

// FIXME: ESEMU SHOWS UP AS UNRESOLVED TYPE MEANING IT JUST ALIASES TO ANY
//        I TRIED REFERENCING A.OUT.D.TS IN TSCONFIG, BUT STILL NO LUCK
//        MAYBE WE JUST NEED TO MAKE IT A REGULAR TS FILE AND IMPORT IT?
//        OR SOMETHING IS WRONG WITH THE CONFIG?
interface Window {
    emuModule: MainModule;
    emulator: ESEmu;
    lastRenderTime: number;
    frameDelta: number;
    frameQueue: [Uint8Array];
}
