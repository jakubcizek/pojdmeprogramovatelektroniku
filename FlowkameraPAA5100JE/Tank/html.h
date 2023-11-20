const char html[] = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Zivebot (Test PAA5100JE)</title>
    <link rel="icon" type="image/x-icon" href="/favicon.ico">
    <style>
        canvas {
            border: 1px solid black;
        }
    </style>
    <script src="/app.js"></script>
</head>
<body>
    <canvas id="canvas"></canvas>
    <div>
        <label for="scale">Meritko 1:</label><input type="number" min="1" max="1000" value="10" id="scale" style="width: 60px;">&nbsp;
        <label for="led">LED flowkamery: </label><input type="checkbox" id="led" checked />&nbsp;
        <input type="button" id="orireset" value="Resetuj orientaci" />
    </div>
</body>
</html>
)html";
