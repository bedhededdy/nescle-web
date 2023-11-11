// TODO: WE WILL NEED TO HAVE A USEEFFECT HERE THAT FOCUSES THE CANVAS (DON'T FORGET TO TAB INDEX)
//       THIS WILL FOCUS IT ON LOAD AND ALSO ON REF.CURRENT CHANGE (MEANING WHEN THE PLACEHOLDER TEXT)
//       TELLING YOU TO INSERT ROM IS REPLACED, THE FOCUS WILL ALSO GO HERE
import React, { useCallback, useEffect, useRef, useState } from "react";
import Speakers from "../Speakers.ts";

import "./styles/EmuScreen.scss";

interface IEmuScreenProps {
    canvasRef: React.RefObject<HTMLCanvasElement>;
    gameInserted: boolean;
}

const EmuScreen: React.FC<IEmuScreenProps> = (props) => {
    const nativeWidth = 256;
    const nativeHeight = 240;

    const [scaledWidth, setScaledWidth] = useState(nativeWidth);
    const [scaledHeight, setScaledHeight] = useState(nativeHeight);

    const [speakers, _setSpeakers] = useState(new Speakers());

    const screenContainerRef = useRef<HTMLDivElement>(null);

    const scaledCanvasRef = props.canvasRef;
    const nativeCanvasRef = useRef<HTMLCanvasElement>(null);

    // FIXME: NOT SURE IF THIS NEEDS TO BE A CALLBACK OR WHAT
    // NOT SURE IF THE EVENT LISTENER IS ACTUALLY BEING REMOVED
    // OR IF THIS FUNCTION IS BEING RECONSTRUCTED EACH TIME OR IF THAT
    // IS NOT OGING TO REMOVE THE EVENT LISTENER, ETC.
    const handleResize = () => {
        // FIXME: THERE IS PROBABLY A BUG HERE THAT ISN'T TAKING INTO ACCOUNT
        // THE BORDER
        // WE NEED TO TAKE INTO ACCOUNT THE BORDERS
        const availWidth = screenContainerRef.current!.clientWidth;
        const availHeight = screenContainerRef.current!.clientHeight;

        const widthScaleFactor = Math.floor(availWidth / nativeWidth);
        const heightScaleFactor = Math.floor(availHeight / nativeHeight);
        const scaleFactor = Math.min(widthScaleFactor, heightScaleFactor);

        setScaledWidth(nativeWidth * scaleFactor);
        setScaledHeight(nativeHeight * scaleFactor);
    }

    useEffect(() => {
        // Make sure we size the canvas correctly on load
        handleResize(); // FIXME: Theoretically the screenContainerRef could be null, so it should probably be in its own use effect

        // Set a resize listener on the window and set our vars appropriately
        window.addEventListener("resize", handleResize);

        // speakers.start();

        // Begin frameRender loop
        onAnimationFrame();

        return () => {
            window.removeEventListener("resize", handleResize);
            speakers.stop();
        }
    }, []);

    useEffect(() => {
        focusCanvas();
        if (props.gameInserted) {
            speakers.start();
        }
    }, [props.gameInserted]);

    const focusCanvas = useCallback(() => {
        scaledCanvasRef.current?.focus();
    }, [scaledCanvasRef.current]);

    // PROBABLY DON'T NEED TO RECONSTRUCT CALLBACK EACH TIME
    const keyDownCallback = useCallback((e: any) => {
        window.emulator.keyDown(e.key)
    }, []);

    const keyUpCallback = useCallback((e: any) => {
        window.emulator.keyUp(e.key);
    }, []);

    const onAnimationFrame = useCallback(() => {
        const t0 = Date.now();
        const emu = window.emulator;
        // FIXME:
        // If we run too fast, we never underflow the buffer, meaning that this doesn't
        // get called enough before we need more samples
        // Best option is to cache a frame of audio and video and playback from that
        // and then just always be rendering the "next frame" instead of the current frame
        // We will always be 1 frame of audio and video behind, but this fixes our issue of having
        // no samples when audio gets called and then skipping over the clock because we generated the frame
        // on the audio thread trying to fill the buffer. Our current approach just forces perpetual starvation
        // after the first underrun. Right now we never underflow because we gen needed samples on the audio callback, but
        // we get choppy video because we don't clock if the audio thread is the one that gets the frame complete signal from the nes.
        // I believe that eventually the audio is always generating the frame, since we always underflow which is an issue since audio
        // thread is not in sync with the frame refresh meaning that we get the new frame but don't render it for awhile.
        let newFrameRendered = false;
        if (window.frameDelta >= 16.67) {
            // FIXME: SEE HERE AND SPEAKERS FOR FIXES

            // FIXME: NO MATTER WHAT HAPPENS, THIS IS NOT ABLE TO CONSISTENTLY
            // PROVIDE AUDIO SAMPLES BEFORE THE QUEUE RUNS OUT BECAUSE WE DO NOT
            // HAVE ANY CONTROL OVER SEQUENCING
            // MAYBE WE SHOULD USE THE TWO THREAD AUDIO MECHANISM AND HAVE A SIGNALING
            // MECHANISM SO THAT THE AUDIO THREAD CAN POTENTIALLY CLOCK THE CPU
            // WHEN NEEDED TO GET MORE SAMPLES
            // ALTERNATIVELY, WE CAN RENDER FRAMES IN ADVANCE AND CACHE THEM
            // AND THEN WE WOULD PRETTY MUCH BE CLOCKING THE "NEXT FRAME"
            // AND ALWAYS BE SHOWING A STALE FRAME AND STALE AUDIO GIVING US SOME LATENCY
            // LOOK AT HOW JSNES-WEB HANDLES THIS (ALTHOUGH HE IS USING SINGLE-THREADED AUDIO W/ DEPRECATED API)

            // FIXME: UNHARDCODE IN THE CPP THE SAMPLE RATE
            // WE NEED TO ACCOUNT FOR MULTIPLE SAMPLE AND FRAMERATES, NOT JUST WHAT WORKS ON MY MACHINE

            // FIXME: NO MATTER HOW MUCH RINGBUFFER I GIVE, WE EVENTUALLY ALWAYS UNDERFILL
            // NOT SURE IF THIS IS BECAUSE OF A ROUDNING DISCREPANCY (SOMETIMES WE GENERATE 798, OTHERS 799)
            // OR IF IT'S JUST BECAUSE OF SEQUENCING, OR IF IT'S BECAUSE WE JUST ARE TOO SLOW
            let samplesProvided = 0;
            if (emu.getRunEmulation()) {
                while (!emu.getFrameComplete()) {
                    // FIXME: WE NEED A MUTEX HERE TO AVOID
                    // OVERWRITING WHILE WE PLAY, ALTHOUGH
                    // THE BUFFER SHOULD BE BIG ENOUGH TO
                    // AVOID THIS ISSUE

                    const sample = emu.emulateSample();
                    speakers.writeSample(sample);
                    // speakers.writeSample(sample);
                    samplesProvided++;
                }
                emu.clearFrameComplete();
                window.frameQueue.push(emu.getFrameBuffer());
                newFrameRendered = true;
            } else {
                // This will at least give us a little buffer so we don't underrun
                // we will be desynced, but not by more than 2 seconds due to size of ring buffer
                for (let i = 0; i < 735; i++) {
                    // This is only true on 60fps at 44.1khz
                    speakers.writeSample(0);
                }



                // window.frameQueue.push(new Uint8Array(256 * 240 * 4));
            }
            // FIXME: NEED TO TRANSFER TO A TIME DELTA APPROACH INSTEAD OF LAST TIME
            // THIS IS HOW WE END UP LOSING TIME
            // window.lastRenderTime -= t0;
            window.frameDelta -= 16.67;
        }

        // FIXME: BAD BUG WHERE WE DON'T PAUSE WHEN LOADING NEW ROM SO THIS
        // CAN TRIGGER BEFORE LOADING IS DONE

        // FIXME: ANOTHER BAD BUG IN THIS BECAUSE IF YOU TAB OFF THIS NEVER GETS CALLED
        // SO THE FRAMEDELTA NEVER GETS UPDATED AND WE RENDER 8 BAJILLION FRAMES TO CATCH UP
        // SINCE WE MISSED SO MUCH TIME
        // EITHER CHANGE YOUR APPROACH OR INVESTIGATE IF THERE IS ANYWAY TO FIGURE OUT WHEN WE
        // GET DETABBED SO WE CAN CLEAR OUR DELTA OR PAUSE THE EMULATION OR MAKE SURE THE DIFFERENCE
        // BETWEEN DATE.NOW AND LASTRENDERTIME IS NEVER TOO BIG

        // FIXME: THERE IS A BAD BUG WHERE IF WE LOAD MEGAMAN2 AND THEN SILVER SURFER IT CRASHES
        // BECAUSE IT CLAIMS IT CAN'T WRITE TO CHRRAM, DOESN'T HAPPEN IF YOU INITIALLY LOAD SILVER SUFER. NOT SURE THE
        // CAUSE OF THE ISSUE

        // window.frameDelta += Date.now() - window.lastRenderTime;

        // FIXME: I AM MAKING VIDEO UNSMOOTH WHEN DEALING WITH AUDIO
        // TAKING AUDIO OUT OF THE CONDITION FIXES THE ISSUE
        // if (emu.getRunEmulation() && (window.frameQueue.length <= 5 || speakers.size() < 4096)) {
        if (emu.getRunEmulation() && (window.frameQueue.length <= 3)) {
            console.log("calling");
            while (!emu.getFrameComplete()) {
                const sample = emu.emulateSample();
                speakers.writeSample(sample);
                // samplesProvided++;
            }
            emu.clearFrameComplete();
            window.frameQueue.push(emu.getFrameBuffer());
            // newFrameRendered = true;
        }

        // FIXME: SOUND IS FINE NOW, BUT THE VIDEO GETS CHOPPY
        // MY GUESS IS THAT EITHER THE VIDEO IS DESYNCING FROM THE "VSYNC"
        // OR SOME FRAMES GET SHOWN FOR TOO SHORT OR WE SKIP A FRAME LEADING TO CHOPPY VIDEO
        // ACTUALLY I THINK THE REAL ISSUE IS THAT WE DON'T CONTROL AUDIO SEQUENCING
        // MEANING THAT WE ALWAYS RUN IN OUR FALLBACK TO AVOID UNDERFLOW AND THAT SOMEHOW
        // IS MAKING OUR VIDEO CHOPPY
        // BASICALLY AUDIO IS REQUESTING TOO MUCH BEFORE W ECAN GIVE AND WE ARE ALWAYS IN DEBT
        // WE SHOULD TEST AND GET FRAMEQUEUING WORKING BEFORE WE DEAL WITH AUDIO ISSUES

        // FIXME: I HAVE PROVED THAT OUR AUDIO QUEUE UP ATTEMPT IS CAUSING THE UNSMOOTHNESS

        if (window.emulator.getRunEmulation() && newFrameRendered) {
            // const pxArr = window.frameQueue.shift();
            let pxArr;
            // console.log("framequeue len: " + window.frameQueue.length);
            if (newFrameRendered) {
                pxArr = window.frameQueue.shift();
            } //else {
                // FIXME: MAYBE INSTEAD OF DOING THIS WE SHOULD
               // pxArr = window.frameQueue[0];
            //}
            if (!pxArr) {
                console.log("framebuffer ran out");
                return;
            }

            const nativeCanvas = nativeCanvasRef.current;
            const scaledCanvas = scaledCanvasRef.current;

            if (nativeCanvas && scaledCanvas) {
                const nativeContext = nativeCanvas.getContext("2d");
                const nativeImgData = nativeContext!.createImageData(nativeWidth, nativeHeight);
                nativeImgData.data.set(pxArr);
                nativeContext!.putImageData(nativeImgData, 0, 0, 0, 0, nativeWidth, nativeHeight);

                const scaledContext = scaledCanvas.getContext("2d");
                // FIXME: THERE MAY BE A COUPLE OTHER PROPS WE NEED
                // TO SET FOR COMPATIBILITY THAT TYPESCRIPT DOESN'T SEE
                // WE MAY NEED TO TSIGNORE OR SOMETHING
                scaledContext!.imageSmoothingEnabled = false;

                // Can't use scaledWidth/height since they for wahtever reason always have the initial value
                // so we need to get it from the canvas itself
                const drawWidth = scaledCanvas.width;
                const drawHeight = scaledCanvas.height;
                scaledContext!.drawImage(nativeCanvas, 0, 0, nativeWidth, nativeHeight, 0, 0, drawWidth, drawHeight);
            }
        }

        window.frameDelta += Date.now() - window.lastRenderTime;
        window.lastRenderTime = Date.now();

        window.requestAnimationFrame(onAnimationFrame);
    }, []);

    return (
        <div ref={screenContainerRef} className="screenContainer">
            {/* FIXME: WE NEED SOME SPACING AWAY FROM THE NAVBAR WHETHER ON THE DIV OR THE CANVAS ITSELF*/}
            <canvas ref={nativeCanvasRef} style={{"display": "none"}} width={nativeWidth} height={nativeHeight} />
            {
                !props.gameInserted ? <p>Insert a ROM</p>
                    : <canvas className="scaledScreen" ref={scaledCanvasRef}
                        tabIndex={0} width={scaledWidth} height={scaledHeight}
                        onClick={focusCanvas} onKeyDown={keyDownCallback} onKeyUp={keyUpCallback} />

            }
        </div>
    );
}

export default EmuScreen;
