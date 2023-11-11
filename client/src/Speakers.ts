import RingBuffer from "ringbufferjs";

export default class Speakers {
    private _buffer: RingBuffer<number>;
    private _audioContext: AudioContext | undefined;
    private _scriptNode: ScriptProcessorNode | undefined;
    private _started: boolean = false;

    private _t: number = 0;

    public get started() { return this._started; }
    public set started(value: boolean) { this._started = value; }

    constructor() {
        // Stores 2 onaudioprocess calls worth of samples (at 48khz this means we will be about 1/6 of a
        // second out of sync with the video)
        this._buffer = new RingBuffer<number>(8192);
    }

    public start(): void {
        this._audioContext = new window.AudioContext();
        // FIXME: NEEDS TO BE 1024 OR 512 FOR ACCURACCY THIS IS JUST FOR OVERKILL
        // FIXME: WE ARE CONSTANTLY UNDERRUNNING THIS BUFFER AT 1024
        this._scriptNode = this._audioContext.createScriptProcessor(4096, 0, 1);
        this._scriptNode.onaudioprocess = this.onAudioProcess.bind(this);
        this._scriptNode.connect(this._audioContext.destination);

        console.log("Sample Rate: " + this._audioContext.sampleRate);
        this._started = true;
    }

    public stop(): void {
        if (this._scriptNode) {
            this._scriptNode.disconnect();
            this._scriptNode.onaudioprocess = null;
            this._scriptNode = undefined;
        }
        if (this._audioContext) {
            this._audioContext.close().catch((e) => {
                console.error("Error clsoing: " + e);
            })
            this._audioContext = undefined;
        }

        this._started = false;
    }

    public writeSample(sample: number): void {
        this._buffer.enq(sample);
    }

    public size(): number {
        return this._buffer.size();
    }

    private onAudioProcess(e: AudioProcessingEvent): void {
        const output = e.outputBuffer.getChannelData(0);
        let samples;
        try {
            samples = this._buffer.deqN(output.length);
        } catch (err) {
            // FIXME: WE WILL NEVER RECOVER FROM THIS BECAUSE WE DON'T
            // FILL THE BUFFER WITH EMPTY SAMPLES
            // WE JUST OUTPUT THEM
            // THEREFORE, THE BUFFER WILL ALWAYS BE UNDERRUN AFTER THE FIRST TIME IT HAPPENS
            console.log("buffer underrun with length: " + this._buffer.size());
            if (window.emulator.getRunEmulation()) {
                samples = this._buffer.deqN(this._buffer.size()); // Dequeue whatever we can
                for (let i = 0; i < samples.length; i++) {
                    output[i] = samples[i];
                }

                // // FIXME: THIS APPROACH GIVES SMOOTH AUDIO, BUT VIDEO CAN BE CHOPPY
                // // MY GUESS IS THAT IN A LOT OF CASES WE ARE ACTUALLY RENDERING A NEW FRAME AND THEN
                // // WHEN WE TRY TO CLOCK WE ACTUALLY SKIP GENERATION, MEANING THAT WE ARE ALWAYS
                // // GETTING AUDIO FROM HERE SINCE WE WILL SKIP CLOCKING IN THE DRAW CALL. OUR GOAL SHOULD NOT TO BE HANDLING EVERYTHING, BUT MERELY PREVENTING THE AUDIO BUFFER FROM STARVING.
                // // I THINK THE BETTER APPROACH IS KEEPING A FRAMECACHE AND AUDIOCACHE AND PLAYING FROM THAT
                // // AND THEN TOPPING THOSE UP WHEN WE GET LOW
                // // IT COULD ALSO BE THAT WE ACTUALLY BLOCK THE FRAME RENDERING BY EATING THEIR CYCLES
                // // AND STARVING THEM
                // // THEREFORE, I THINK THE BEST APPROACH IS FRAMECACHING AND AUDIOCACHING SO THAT
                // // WE DON'T HAVE THESE SEQUENCING ISSUES
                // // ALTERNATIVELY WE COULD JUST FORCE THE EMULATOR TO DO A CERTAIN NUMBER OF CLOCKS EACH FRAME
                // // INSTEAD OF WAITING FOR THE FRAME COMPLETE SIGNAL (IF WE END UP ON A FRAME GENERATION CYCLE HERE, WE
                // // WILL STOP THE MAIN CLOCKER FROM RENDERING THE FRAME ON THIS DRAW CALL)

                // // Generate enough samples to fill the buffer and then generate 2048 more
                for (let i = 0; i < output.length - samples.length + 2048; i++) {
                    this.writeSample(window.emulator.emulateSample());
                }

                const leftoverSamples = this._buffer.deqN(output.length - samples.length);
                for (let i = samples.length; i < output.length; i++) {
                    output[i] = leftoverSamples[i - samples.length];
                }
            } else {
                for (let i = 0; i < output.length; i++) {
                    output[i] = 0;
                }
            }

            return;
        }
        for (let i = 0; i < output.length; i++) {
            output[i] = samples[i];
        }

        // this works, meaning that our audio from emu is wrong or we process it wrong
        // or that this is too slow on one thread
        // const period = 48000 / 440;
        // for (let i = 0; i < output.length; i++) {
        //     output[i] = this._t > period / 2 ? 0.5 : -0.5;
        //     this._t = (this._t + 1) % period;
        // }
    }
}
