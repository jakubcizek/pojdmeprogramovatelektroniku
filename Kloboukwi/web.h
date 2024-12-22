const char* html = R"HTML(

<!DOCTYPE html>
<html lang="cs">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Fira+Code:wght@300..700&display=swap" rel="stylesheet">

  <title>Kloboukwi</title>
  <style>
    body {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      font-family: Arial, sans-serif;
    }
    #mainContainer {
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100vh;
    }

    /* Kontejner pro obsah a SVG */
    #contentContainer, #svgContainer {
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    /* Horizontální rozložení pro obsah */
    #contentContainer {
      margin-right: 50px;
    }

    /* Nastavení velikosti SVG */
    #svgContainer {
      width: 200px;
      height: auto;
    }

    #svgCanvas {
      width: 100%;
      height: 100%;
    }
    h1 {
      text-align: center;
      margin-bottom: 20px;
    }
    .button-container {
      display: flex;
      justify-content: center;
      margin-bottom: 15px;
    }
    button {
      width: 150px;
      margin: 0 5px;
    }
    input[type=text]{
      width: 145px;
      margin: 0 5px;
    }
    #status, #messages {
      width: 100%;
      max-width: 800px;
      border: 1px solid #ccc;
      padding: 10px;
      margin-bottom: 10px;
    }
    #status {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      text-align: left;
      font-weight: bolder;
      font-size: 0.9em;
    }
    #messages {
      height: 250px;
      overflow-y: auto;
      font-size: 0.9em;
      font-family: "Fira Code"
    }
  </style>
</head>
<body>
  <div id="mainContainer">
  <div id="contentContainer">
  <h1>Kloboukwi 🐶</h1>

  <div class="button-container">
    <button id="openBtn">Otevřít</button>
    <button id="closeBtn">Zavřít</button>
    <button id="openStepBtn">Otevřít o kousek</button>
    <button id="closeStepBtn">Zavřít o kousek</button>
  </div>
  <div class="button-container">
    <input type="text" id="stepInput" placeholder="Kousek">
    <button id="setStepBtn">Nastavit kousek</button>
    <input type="text" id="motorMaxSpeedInput" placeholder="Rychlost">
    <button id="motorMaxSpeedBtn">Nastavit rychlost</button>
  </div>
  <div class="button-container">
    <button id="resetHomeBtn">Resetuj pozici</button>
    <button id="setNewOpen">Resetovat otevřeno</button>
    <button id="resetDriver">Resetovat driver</button>
    <button id="resetDriver">Resetovat EEPROM</button>
  </div>
  <div class="button-container">
    <button id="getStatusBtn">Získej stav</button>
    <button id="readEEPROMBtn">Vypiš EEPROM</button>
    <button id="uploadFileBtn">Upgrade FW</button>
    <input type="file" id="fileInput" style="display: none;">
    <button id="restartBtn">Restart ESP8266</button>
  </div>
  <div class="button-container">
    <input type="text" id="gotoInput" placeholder="Pozice">
    <button id="gotoBtn">Na pozici</button>
    <button id="lockBtn">Přepnout zámek</button>
    <button id="buzzBtn">Bzučák</button>
  </div>

  <div id="status">
    <div id="build">Build:</div>
    <div id="resolution">Rozlišení:</div>
    <div id="state">Stav:</div>
    <div id="pos">Pozice:</div>
    <div id="openpos">Otevřeno:</div>
    <div id="closepos">Zavřeno:</div>
    <div id="ir_stop">IR doraz:</div>
    <div id="ir_guide">IR hřeben:</div>
    <div id="btn">Tlacitko:</div>
    <div id="oc">OC:</div>
    <div id="lock">Zámek:</div>
  </div>

  <div id="messages"></div>
  </div>
  <!-- SVG Kontejner -->
    <div id="svgContainer">
      <svg id="svgCanvas"></svg>
    </div>
  </div>

  <script>
    const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
    const socket = new WebSocket(`${protocol}${window.location.hostname}:81`);
    const dombuild = document.getElementById("build");
    const domoc = document.getElementById("oc");
    const domup = document.getElementById("up");
    const domstate = document.getElementById("state");
    const dompos = document.getElementById("pos");
    const domirstop = document.getElementById("ir_stop");
    const domirguide = document.getElementById("ir_guide");
    const domopenpos = document.getElementById("openpos");
    const domclosepos = document.getElementById("closepos");
    const dommanualstep = document.getElementById("stepInput");
    const dommaxspeed = document.getElementById("motorMaxSpeedInput");
    const domres = document.getElementById("resolution");
    const domlock = document.getElementById("lock");
    const dombtn = document.getElementById("btn");
    let countdown = 15; 
    let countdownTimer = null;

    function countdownAfterUpgrade(){
      logMessage("Restart za " + (--countdown) + " sekund...");
      if(countdown == 0){
        clearInterval(countdownTimer);
        location.reload(true);
      }
    }

    let position = 0;

    socket.onopen = () => {
      logMessage("Spojení otevřeno, čekám na data...");
    };

    socket.onmessage = (event) => {
      const timestamp = getCurrentTime();
      logMessage(`${timestamp} - ${event.data}`);
      if(event.data.startsWith("STATUS")){
        const chops = event.data.split(" ");
        const state = chops[1];
        const pos = chops[2];
        const irstop = chops[3];
        const irguide = chops[4];
        const openpos = chops[5];
        const closepos = chops[6];
        const manualstep = chops[7];
        const maxspeed = chops[8];
        const lock = chops[9];
        const btn = chops[10];
        position = pos;
        domstate.innerHTML = "Stav: " + decodeState(parseInt(state)) + " (" + state + ")";
        dompos.innerHTML = "Pozice: " + pos;
        domirstop.innerHTML = "IR doraz: " + irstop;
        domirguide.innerHTML = "IR hřeben: " + irguide;
        dommaxspeed.value = maxspeed;
        dommanualstep.value = manualstep;
        domopenpos.innerHTML = "Otevřeno: " + openpos;
        domclosepos.innerHTML = "Zavřeno: " + closepos;
        domlock.innerHTML = "Zámek: " + lock;
        dombtn.innerHTML = "Tlačítko: " + ((btn==1)? 0:1);
        if(state != 0xff && pos >= 0 && pos <= open){
          set_window_twin_angle(pos, openpos);
        }else{
          set_window_twin_angle(-1, -1);
        }
      }else if(event.data.startsWith("BUTTON")){
        const chops = event.data.split(" ");
        const btn = chops[1];
        dombtn.innerHTML = "Tlačítko: " + ((btn==1)? 0:1);
      }else if(event.data.startsWith("RES")){
        const chops = event.data.split(" ");
        const res = chops[1];
        domres.innerHTML = "Rozlišení: " + res + " μkroků";
      }else if(event.data.startsWith("BUILD")){
        const chops = event.data.split(" ");
        const build = chops.slice(1).join(" ");
        dombuild.innerHTML = "Build: " + build;
      }else if(event.data.startsWith("OC")){
        const chops = event.data.split(" ");
        const oc = chops[1];
        domoc.innerHTML = "OC: " + oc + " mA";
      }else if(event.data.startsWith("UPGRADE")){
        const chops = event.data.split(' ');
        if(chops[1] == "OK"){
          countdownTimer = setInterval(countdownAfterUpgrade, 1000);
        }
      }
    };

    socket.onclose = () => {
      logMessage('Spojení s WS ukončeno.');
    };

    function decodeState(state){
      let retval =  "[" + state + "]";
      switch(state){
        case 0:
        retval = "Neznámý";
        break;
        case 1:
        retval = "Zavřít";
        break;
        case 2:
        retval = "Zavírám";
        break;
        case 3:
        retval = "Zavřeno";
        break;
        case 4:
        retval = "Otevřít";
        break;
        case 5:
        retval = "Otevírám";
        break;
        case 6:
        retval = "Otevřeno";
        break;
        case 7:
        retval = "Otevřít o krok";
        break;
        case 8:
        retval = "Otevírám o krok";
        break;
        case 9:
        retval = "Otevřeno o krok";
        break;
        case 10:
        retval = "Zavřít o krok";
        break;
        case 11:
        retval = "Zavírám o krok";
        break;
        case 12:
        retval = "Zavřeno o krok";
        break;
        case 13:
        retval = "Zavřít k dorazu";
        break;
        case 14:
        retval = "Zavírám k dorazu";
        break;
        case 15:
        retval = "Zavřeno k dorazu";
        break;
        case 0xfe:
        retval = "Chybí hřeben";
        break;
        case 0xff:
        retval = "Neznámý";
        break;
        default:
        retval = state;
        break;
      }
      return retval;
    }

    function logMessage(message) {
      const messagesDiv = document.getElementById('messages');
      const messageElement = document.createElement('div');
      messageElement.textContent = message;
      messagesDiv.appendChild(messageElement);
      messagesDiv.scrollTop = messagesDiv.scrollHeight;
    }

    function getCurrentTime() {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      const seconds = String(now.getSeconds()).padStart(2, '0');
      const milliseconds = String(now.getMilliseconds()).padStart(3, '0');
      return `${hours}:${minutes}:${seconds}.${milliseconds}`;
    }

    document.getElementById('getStatusBtn').addEventListener('click', () => {
      sendMessage('status');
    });

    document.getElementById('resetHomeBtn').addEventListener('click', () => {
      sendMessage('rsthome');
    });

    document.getElementById('openBtn').addEventListener('click', () => {
      sendMessage('open');
    });

    document.getElementById('setNewOpen').addEventListener('click', () => {
      sendMessage('SO' + position);
    });

    document.getElementById('resetDriver').addEventListener('click', () => {
      sendMessage('RD');
    });

    document.getElementById('closeBtn').addEventListener('click', () => {
      sendMessage('close');
    });

    document.getElementById('openStepBtn').addEventListener('click', () => {
      sendMessage('MO');
    });

    document.getElementById('closeStepBtn').addEventListener('click', () => {
      sendMessage('MC');
    });

    document.getElementById('readEEPROMBtn').addEventListener('click', () => {
      sendMessage('RE');
    });

    document.getElementById('lockBtn').addEventListener('click', () => {
      sendMessage('LK');
    });

    document.getElementById('setStepBtn').addEventListener('click', () => {
      const val = document.getElementById('stepInput').value;
      if (val) {
        sendMessage('SS' + val);
      }
    });

    document.getElementById('gotoBtn').addEventListener('click', () => {
      const val = document.getElementById('gotoInput').value;
      if (val) {
        sendMessage('GO' + val);
      }
    });

    document.getElementById('motorMaxSpeedBtn').addEventListener('click', () => {
      const val = document.getElementById('motorMaxSpeedInput').value;
      if (val) {
        sendMessage('MMS' + val);
      }
    });

    function sendMessage(msg) {
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(msg);
      } else {
        logMessage('WS není připojeno');
      }
    }

    // Funkce pro nahrání souboru
    document.getElementById('uploadFileBtn').addEventListener('click', () => {
      document.getElementById('fileInput').click();
    });

    document.getElementById('fileInput').addEventListener('change', function() {
      const file = this.files[0];
      if (file) {
        const formData = new FormData();
        formData.append("file", file);
        logMessage("Posílám firmware na čip...");

        fetch('/update', {
          method: 'POST',
          body: formData
        })
        .then(response => response.text())
        .then(data => {
        })
        .catch(error => {
        });
      }
    });

    function adjustSvgHeight() {
      const contentHeight = document.getElementById('contentContainer').offsetHeight - 15;
      document.getElementById('svgContainer').style.height = contentHeight + 'px';
    }

    function set_window_twin_angle(value, maximum) {
      adjustSvgHeight();
      
      const svgCanvas = document.getElementById("svgCanvas");

      // Dynamické rozměry na základě velikosti SVG
      const svgWidth = svgCanvas.getBoundingClientRect().width;
      const svgHeight = svgCanvas.getBoundingClientRect().height;
      const wallStrength = 10; // Tloušťka zdi a podlahy

      const wallHeight = svgHeight - wallStrength; // Výška zdi, zbytek výšky po podlaze

      // Vymazat SVG obsah, pokud existuje
      svgCanvas.innerHTML = "";

      // Vytvořit obdélník představující zeď domu
      const wall = document.createElementNS("http://www.w3.org/2000/svg", "rect");
      wall.setAttribute("x", 0);
      wall.setAttribute("y", 0);
      wall.setAttribute("width", wallStrength);
      wall.setAttribute("height", wallHeight);
      wall.setAttribute("fill", "#d4f3fc");
      svgCanvas.appendChild(wall);

      // Vytvořit obdélník představující podlahu
      const floor = document.createElementNS("http://www.w3.org/2000/svg", "rect");
      floor.setAttribute("x", 0);
      floor.setAttribute("y", wallHeight);
      floor.setAttribute("width", svgWidth);
      floor.setAttribute("height", wallStrength);
      floor.setAttribute("fill", "#f2f0e1");
      svgCanvas.appendChild(floor);


      // Vytvořit tečkovanou čáru pro maximum (45 stupňů), začínající na podlaze
      const maxLine = document.createElementNS("http://www.w3.org/2000/svg", "line");
      maxLine.setAttribute("x1", wallStrength);
      maxLine.setAttribute("y1", wallHeight);
      maxLine.setAttribute("x2", wallStrength);
      maxLine.setAttribute("y2", 0);
      maxLine.setAttribute("stroke", "#ffd6d1");
      maxLine.setAttribute("stroke-width", 1);
      maxLine.setAttribute("stroke-dasharray", "5,5");
      maxLine.setAttribute("transform", `rotate(15, ${wallStrength}, ${wallHeight})`);
      svgCanvas.appendChild(maxLine);

      if(value > -1 && maximum > -1){
        // Vytvořit rotující čáru (zelenou), která začíná na podlaze
        const rotatingLine = document.createElementNS("http://www.w3.org/2000/svg", "line");
        rotatingLine.setAttribute("x1", wallStrength);
        rotatingLine.setAttribute("y1", wallHeight);
        rotatingLine.setAttribute("x2", wallStrength);
        rotatingLine.setAttribute("y2", 0);
        rotatingLine.setAttribute("stroke", "black");
        rotatingLine.setAttribute("stroke-width", 4);
        svgCanvas.appendChild(rotatingLine);

        // Výpočet úhlu rotace pro zelenou čáru
        const maxAngle = 15; // Maximální úhel v stupních
        const angle = (value / maximum) * maxAngle; // Úhel na základě hodnoty

        // Nastavení rotace pro rotující čáru s bodem otáčení na podlaze vedle zdi
        if(value == maximum){
          rotatingLine.setAttribute("stroke", "#83bd46");
        }
        if(value == 0){
          rotatingLine.setAttribute("stroke", "#d65c47");
        }
        rotatingLine.setAttribute("transform", `rotate(${angle}, ${wallStrength}, ${wallHeight})`);
      }
    }

  </script>
</body>
</html>


)HTML";
