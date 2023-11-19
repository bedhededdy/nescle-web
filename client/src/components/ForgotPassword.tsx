import React, { useCallback, useRef, useState } from "react";

const ForgotPassword: React.FC = () => {
    // TODO: SET A TIMEOUT TO SHOW A FORM ELEMENT TO RESEND THE EMAIL
    const [invalidEmail, setInvalidEmail] = useState<boolean | null>(null); // null will be used to indicate the field is empty
    const [submitEnabled, setSubmitEnabled] = useState(false);
    const [emailSent, setEmailSent] = useState<boolean | null>(null);

    const emailRef = useRef<HTMLInputElement>(null);
    const forgotPasswordRef = useRef<HTMLButtonElement>(null);

    const emailOnChange = useCallback(() => {
        const emailAddress = emailRef.current!.value;
        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        const validEmail = emailRegex.test(emailAddress);

        if (!validEmail && emailAddress !== "") {
            setInvalidEmail(true);
            setSubmitEnabled(false);
            emailRef.current!.classList.add("errInput");
        } else {
            setInvalidEmail(false);
            setSubmitEnabled(emailAddress !== "");
            emailRef.current!.classList.remove("errInput");
        }
    }, [emailRef.current]);

    const forgotPasswordCallback = useCallback(() => {
        fetch(window.apiUrl + "/api/forgot-password", {
            method: "POST",
            credentials: "include",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({email: emailRef.current!.value})
        }).then((res) => {
            setEmailSent(res.ok);
            setSubmitEnabled(false);
            setTimeout(() => {
                setSubmitEnabled(true);
                setEmailSent(null);
            }, 30000);
        }).catch((err) => {
            console.error("Failed to send email: " + err);
            setEmailSent(false);
        });
    }, [emailRef.current]);

    return (
        <div>
            <div>
                <h2>Forgot Password</h2>
                <div>
                    <input ref={emailRef} onChange={emailOnChange} type="text" placeholder="Email" />
                    {invalidEmail ? <p className="errText">Invalid email address</p> : null}
                    <button ref={forgotPasswordRef} disabled={!submitEnabled} onClick={forgotPasswordCallback}>Submit</button>
                </div>
                {emailSent === true ? <p className="succText">Email sent</p> : null}
                {emailSent === false ? <p className="errText">Failed to send email</p> : null}
            </div>
        </div>
    );
}

export default ForgotPassword;
