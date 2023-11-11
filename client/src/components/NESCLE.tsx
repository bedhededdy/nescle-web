import React, { useRef, useState, useCallback } from "react";

import EmuScreen from "./EmuScreen.tsx";
import NavBar from "./NavBar.tsx";

import "./styles/NESCLE.scss";

const NESCLE: React.FC = () => {
    // NOTES:
    // NAVBAR WILL TAKE UP ONLY WHAT IS NECESSARY
    // THE SCREEN WILL TAKE UP THE REST
    // WE WILL GET THE WIDTH DYNAMICALLY SO WE DON'T NEED THE WIDTH AS PROPS
    // WE CAN DO THIS WITH FLEX 1 TO TAKE UP REST OF SPACE
    // WITH BODY AT 100VH

    const [gameInserted, setGameInserted] = useState(false);

    const canvasRef = useRef<HTMLCanvasElement>(null);

    const gameInsertedCallback = useCallback(() => {
        setGameInserted(true);
    }, [setGameInserted]);

    return (
        <div className="nescleContainer">
            <NavBar canvasRef={canvasRef} gameInsertedCallback={gameInsertedCallback} />
            <EmuScreen canvasRef={canvasRef} gameInserted={gameInserted} />
        </div>
    );
}

export default NESCLE;
