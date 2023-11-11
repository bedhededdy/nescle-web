const express = require("express");
const app = express();
// const cors = require("cors");

const PORT = process.env.PORT || 8000;

// only allow requests from our frontend
// app.use(cors({origin: "*"}));

app.get('/', (req, res) => {
    res.json({message: "hello world"});
});

app.listen(PORT, () => {
    console.log("Server running on port " + PORT);
});
