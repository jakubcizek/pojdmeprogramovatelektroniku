const char html[] = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Vozidlo s koly mecanum</title>
    <link rel="icon" type="image/x-icon" href="/favicon.ico">
    <style>
      html, body {
        height: 100%;
        margin: 0;
        display: flex;
        justify-content: center;
        align-items: center;
        font-family: Arial, sans-serif;
      }
    </style>
    <script src="/app.js"></script>
</head>
<body>
  <div>
    Pro ovládání použij směrové šipky na klávesnici<br />
    <label for="speed">Rychlost: </label><input type="number" min="1" max="255" value="255" id="speed" style="width: 60px;">
  </div>
</body>
</html>
)html";