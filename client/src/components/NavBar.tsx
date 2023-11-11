import React, { useCallback, useId, useRef } from "react";

import "./styles/NavBar.scss";

interface INavBarProps {
    canvasRef: React.RefObject<HTMLCanvasElement>;
    gameInsertedCallback: () => void;
}

const NavBar: React.FC<INavBarProps> = (props) => {
    const githubRef = useRef<HTMLAnchorElement>(null);
    const helpRef = useRef<HTMLAnchorElement>(null);
    const aboutRef = useRef<HTMLAnchorElement>(null);
    const chooseFileRef = useRef<HTMLInputElement>(null);
    const titleRef = useRef<HTMLHeadingElement>(null);

    // FIXME: PROPERLY TYPE E
    const chooseFileCallback = useCallback((e: any) => {
        const file = e.target.files[0];
        if (!file) {
            return;
        }

        const fileReader = new FileReader();
        fileReader.onload = (_e) => {
            const emu = window.emulator;
            const buf = fileReader.result as ArrayBuffer;
            const bufPtr = window.emuModule._malloc(buf.byteLength);
            const finalBuf = new Uint8Array(window.emuModule.HEAPU8.buffer, bufPtr, buf.byteLength);
            finalBuf.set(new Uint8Array(buf));

            if (emu.loadROM(finalBuf.byteOffset)) {
                console.log("Load successful");
                // emu.powerOn();
                emu.reset();
                emu.setRunEmulation(true);
            } else {
                console.log("Load failed");
                // emu.setRunEmulation(false);
            }

            window.emuModule._free(bufPtr);

            titleRef.current!.innerText = file.name;
            props.canvasRef.current?.focus();
            props.gameInsertedCallback();
        }
        fileReader.readAsArrayBuffer(file);
    }, []);

    const loadRomCallback = useCallback(() => {
        // Stop the emulation when we want to load a file
        window.emulator.setRunEmulation(false);
        chooseFileRef.current!.click();
    }, [chooseFileRef.current]);

    const resetCallback = useCallback(() => {
        // FIXME: THE ESEMU FUNCTIONS ARE NOT BEING RECOGNIZED HERE
        window.emulator.reset();
    }, []);

    const playPauseCallback = useCallback(() => {
        window.emulator.setRunEmulation(!window.emulator.getRunEmulation());
    }, []);

    const githubCallback = useCallback(() => {
        githubRef.current!.click();
    }, [githubRef.current]);

    const helpCallback = useCallback(() => {
    }, [helpRef.current]);

    const aboutCallback = useCallback(() => {
    }, [aboutRef.current]);


    return (
        <>
            <nav>
                <div className="leftButtonsContainer">
                    <button onClick={loadRomCallback}>Load ROM</button>
                    {/* <button>Save</button> */}
                    {/* <button>Load</button> */}
                    <button onClick={resetCallback}>Reset</button>
                    <button onClick={playPauseCallback}>Play/Pause</button>
                </div>
                <div className="middleSpaceContainer">
                    <div className="headerContainer">
                        <h1 ref={titleRef}>NESCLE</h1>
                    </div>
                </div>
                <div className="rightButtonsContainer">
                    <button onClick={githubCallback}>Github</button>
                    <button onClick={helpCallback}>Help</button>
                    <button onClick={aboutCallback}>About</button>
                </div>

                <div style={{"display": "none"}}>
                    <input type="file" ref={chooseFileRef} onChange={chooseFileCallback} />
                    <a href="https://github.com/bedhededdy/nescle" ref={githubRef} target="_blank"></a>
                    <a href="" ref={helpRef} target="_blank"></a>
                    <a href="" ref={aboutRef} target="_blank"></a>
                </div>
            </nav>
        </>
    );
}

export default NavBar;
