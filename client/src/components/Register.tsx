// FIXME: THIS SHOULD BE COMPLETELY REWRITTEN TO HAVE TWO COMPONENTS
// AN EMAIL ADDRESS INPUT AND A DUAL PASSWORD INPUT
// BOTH COMPONENTS WILL HAVE CALLBACKS THAT TRIGGER EVENTS IN THE PARENT
// LIKE ENABLING THE REGISTER BUTTON
import React, { useRef, useCallback, useState } from "react";
import { Link, useNavigate } from "react-router-dom";

import "./styles/Register.scss";

const Register: React.FC = () => {
    const [registerEnabled, setRegisterEnabled] = useState(false);
    const [passwordsMatch, setPasswordsMatch] = useState<boolean | null>(null);  // null will be used to indicate both the fields are empty
    const [invalidEmail, setInvalidEmail] = useState<boolean | null>(null); // null will be used to indicate the field is empty

    const emailRef = useRef<HTMLInputElement>(null);
    const passwordRef = useRef<HTMLInputElement>(null);
    const confirmPasswordRef = useRef<HTMLInputElement>(null);
    const registerRef = useRef<HTMLButtonElement>(null);

    const navigate = useNavigate();

    const emailOnChange = useCallback(() => {
        // FIXME: THIS SHOULD BE CHECKING FOR PASSWORD EQUALITY BEFORE ENABLING
        // THE REGISTER BUTTON
        // FIXME: THERE SHOULD BE A FUNCTION THAT CHECKS WHAT TO DO BECAUSE
        // PWD IS NOT VALIDATING THE EMAIL SO YOU CAN ACTUALLY SEND AN INVALID
        // EMAIL
        const emailAddress = emailRef.current!.value;
        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        const validEmail = emailRegex.test(emailAddress);

        if (!validEmail && emailAddress !== "") {
            setInvalidEmail(true);
            setRegisterEnabled(false);
            emailRef.current!.classList.add("errInput");
            return;
        } else {
            setInvalidEmail(false);
            emailRef.current!.classList.remove("errInput");
        }

        if (emailAddress && passwordRef.current!.value && confirmPasswordRef.current!.value) {
            setRegisterEnabled(passwordRef.current!.value === confirmPasswordRef.current!.value);
        } else {
            setRegisterEnabled(false);
        }
    }, [emailRef.current]);

    const passwordOnChange = useCallback(() => {
        // FIXME: SET THE ERRINPUT CLASS TO THE PASSWORD FILED
        if (!passwordRef.current!.value || !confirmPasswordRef.current!.value) {
            setPasswordsMatch(null);
            setRegisterEnabled(false);
            return;
        }

        if (passwordRef.current!.value === confirmPasswordRef.current!.value) {
            setPasswordsMatch(true);
            setRegisterEnabled(!!emailRef.current!.value);

            // FIXME: MAY CRASH IF THAT CLASS DOESN'T EXIST
            passwordRef.current!.classList.remove("errInput");
        } else {
            setPasswordsMatch(false);
            setRegisterEnabled(false);

            // FIXME: MAY ADD IT MULTIPLE TIMES
            passwordRef.current!.classList.add("errInput");
        }
    }, [passwordRef.current, confirmPasswordRef.current]);

    const registerCallback = useCallback(() => {
        const email = emailRef.current!.value;
        const password = passwordRef.current!.value;
        const confirmPassword = passwordRef.current!.value;

        if (!email || !password || !confirmPassword) {
            console.log("Email or password is empty");
            return;
        }

        if (password !== confirmPassword) {
            console.log("Passwords do not match");
            return;
        }

        fetch(window.apiUrl + "/api/register", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({email: email, password: password})
        }).then((res) => {
            console.log("Register responded with result: ", res.status);
            if (res.ok)
                navigate("/login");
        }).catch((err) => {
            console.error(err);
        })
    }, [emailRef.current, passwordRef.current]);

    return (
        <div className="registerContainer">
            <div className="registerChild">
                <h2>Register</h2>
                <div>
                    <input ref={emailRef} onChange={emailOnChange} type="text" placeholder="Email" />
                    {invalidEmail === true ? <p className="errText">Invalid Email</p> : null}
                    <input ref={passwordRef} onChange={passwordOnChange} type="password" placeholder="Password" />
                    {passwordsMatch === false ? <p className="errText">Passwords do not match</p> : null}
                    <input ref={confirmPasswordRef} onChange={passwordOnChange} type="password" placeholder="Confirm Password" />
                    <button ref={registerRef} disabled={!registerEnabled} onClick={registerCallback}>Register</button>
                </div>
                <div>
                    <Link to="/login">Return to the Homepage</Link>
                </div>
            </div>
        </div>
    );
}

export default Register;
