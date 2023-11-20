const char js[] = R"js(
let x, y; // Souradnice tanku na platne
let history_x = new Array(1000); // Pro prehlednost kreslime do sceny jen trajektorii o 100 bodech
let history_y = new Array(1000);
let key_states = {}; // Pomocna struktura pro management stisku klaves rizeni tanku 
let path_length = 0; // Celkova delka trasy v bezrozmernych jednotkach
let xy_counter = 0; // Pocitadlo platnych XY zmen
let movement_status = "neznamy" // Status pohybu
let scale = 10; // Meritko zmeny XY promitnute na platno (1:10 ve vychozim stavu)
let tank_orientation = "N";

let socket; // Objekt WebSocket spojeni
let canvas; // Kreslici platno
let ctx; // Kreslici kontext
let tank_image; // Obrazek tanku

const noise_limit = 1; // Pokud dojde k mensi XY zmene, ignoruji jako sum
const ws_tcp_port = 81; // TCP port websocketoveho spojeni
const tank_orinetation_angles = {
    "N": 0,
    "E": 90,
    "S": 180,
    "W": 270
}

// Hlavni funkce setup jako v Arduinu. Zavolame na zacatku programu
function setup() {
    // Nastartovani kresliciho platna a kontextu
    canvas = document.getElementById("canvas");
    ctx = canvas.getContext("2d");
    canvas.width = 1280;
    canvas.height = 720;

    //Rucni nastaveni meritka
    const dom_scale = document.getElementById("scale");
    dom_scale.addEventListener("change", (event)=>{
        scale = parseInt(event.target.value);
        console.log("Zmena meritka na 1:" + scale);
        resetHistory();
    });

    //Rucni nastaveni LED flowkamery
    const dom_led = document.getElementById("led");
    dom_led.addEventListener("change", (event)=>{
        if(event.target.checked){
            console.log("Zapinam LED flowkamery");
            socket.send("CL1");
        }
        else{
            console.log("Vypinam LED flowkamery");
            socket.send("CL0");
        }
    });

    //Rucni reset orientace
    const dom_orireset = document.getElementById("orireset");
    dom_orireset.addEventListener("click", (event)=>{
        socket.send("ORI_RESET");
        tank_orientation = "N";
        console.log("Reset orientace");
    });

    // Resetujeme historii XY souradnic
    resetHistory();

    // Vytvorime ikonu tanku
    tank_image = new Image();
    tank_image.src = "/tank.png";

    // Zaregistrujeme udalosti stiksu a uvolneni klaves
    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);
    
    //Nastartovani WebSocketu
    socket = new WebSocket("ws://" + window.location.hostname + ":" + ws_tcp_port);
    // Pri prichodu dat z WebScoketu
    socket.addEventListener("message", (event) => {
        // Nase data museji mit format COMMAND:ARG1,ARG1,ARG3...
        // Priklad pro XY data: XY:-17,10
        const data = event.data.split(":");
        if(data.length < 2) return;

        const command = data[0];
        const args = data[1];

        // Pokud dorazila XY data
        if (command == "XY") {
            const xy = args.split(",").map(Number);
            if (xy.length == 2) {
                // Pokud flowkamera vraci nenulove hodnoty 
                if(Math.abs(xy[0]) > 0 && Math.abs(xy[1]) > 0){
                    // Pokud jsou hodnoty vetsi nez limit sumu,
                    // jedna se po pohyb, ktery bereme v uvahu
                    if(Math.abs(xy[0]) >= noise_limit && Math.abs(xy[1]) >= noise_limit){
                        movement_status = "pohyb";
                        xy_counter++;
                        const lastX = x;
                        const lastY = y;
                        
                        // Zmenu podelime meritkem a pripocitame k souradnicim XY
                        // Vetsi meritko: v platne se projevi az vetsi pohyb
                        // Mensi meritko: V platne se projevi sebemensi pohyb
                        // V praxi by to chtelo dynamickem meritko/interaktivni zoom,
                        // ale jde nam o jednoduchost prikladu, a tak mame jen fixni 
                        // platno s pevnymi rozmery sceny  
                        x += (xy[0] / scale);
                        y -= (xy[1] / scale);
                        
                        // Spozitame vzdalenost oproti predchozimu mereni a tu celkovou
                        // Stale pracujeme s bezrozmernymi jednotkami
                        path_length += Math.sqrt(Math.pow(x - lastX, 2) + Math.pow(y - lastY, 2));

                        // Omezime souradnice tanku jen na viditelne platno, takze se nam nezajede za okraj 
                        x = Math.max(0, Math.min(x, canvas.width));
                        y = Math.max(0, Math.min(y, canvas.height));

                        // Aktualizujeme historii trajektorie
                        aktualizujHistorii();
                    }
                    // Pokud je relativni zmena pohybu prilis mala,
                    // povazujeme ji za sum
                    else{
                        movement_status = "sum"
                    }
                }
                // Pokud je relativni zmena rovna nule,
                // kamera nedetekuje zadny pohyb
                else{
                    movement_status = "stojim";
                }
                // Predame animacni smycce weboveho prohlizece
                // ke spracovani funkci draw 
                requestAnimationFrame(draw);
            }
        }
        else if(command == "SYS"){
            const message = args.split(",");
            if(message[0] == "ORIENTATION"){
                tank_orientation = message[1];
                console.log("Potvrzeni zmeny orientace tanku na: " + tank_orientation);
            }
        }
    });

    // Predame animacni smycce weboveho prohlizece
    // ke spracovani funkci draw 
    requestAnimationFrame(draw);
}

// Funkce draw prekresli platno
function draw() {
    // Smazeme platno
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Vykreslime sedou barvou trajektorii pohybu z historie
    ctx.strokeStyle = "gray";
    ctx.lineWidth = 3;
    for (let i = 1; i < history_x.length; i++) {
        ctx.beginPath();
        ctx.moveTo(history_x[i-1], history_y[i-1]);
        ctx.lineTo(history_x[i], history_y[i]);
        ctx.stroke();
    }

    // Vykreslime ikonu tanku na pozici XY
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(tank_orinetation_angles[tank_orientation]  * (Math.PI / 180));
    ctx.drawImage(tank_image, -tank_image.width / 2, -tank_image.height / 2);
    ctx.restore();

    // Do leveho horniho rohu vykreslime statistiku
    ctx.fillStyle = "black";
    ctx.font = "16px Arial";
    ctx.fillText("Status: " + movement_status, 10, 20);
    ctx.fillText("Orientace tanku: " + tank_orientation, 10, 40);
    ctx.fillText("Meritko: 1:" + scale, 10, 60);
    ctx.fillText("Trasa: " + path_length.toFixed(0) + " jednotek", 10, 80);
    ctx.fillText("Souradnice tanku: " + x.toFixed(0) + " x " + y.toFixed(0) + " px", 10, 100);
}

// Funkce pro reset historie
// Nastavime vsechny body trajektorie na stred platna
function resetHistory(){
    console.log("Reset historie");
    x = canvas.width / 2;
    y = canvas.height / 2;

    for (let i = 0; i < history_x.length; i++) {
        history_x[i] = x;
        history_y[i] = y;
    }
}


// Funkce pro zpracovani stisknuti klavesy a jejiho drzeni
function onKeyDown(event) {

    // Pokud stikneme R, resetujeme historii
    // Tank se vycentruje na stred
    if (event.key == "R" || event.key == "r"){
        resetHistory();
    }

    // Smerove sipky ridi tank
    // Pri stisknuti a drzeni se poslou skrze WebSocket povely FORWARD, BACKWARD, LEFT, RIGHT
    // Aby se pri stisku a drzeni neposilaly zpravy stale dokola, ale jen poprve,
    // v key_states kontrolujeme jestli uz klavesy nahodou nedrzime a v tom pripade z funcke vyskocime
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(event.key)) {
        if (key_states[event.key]) {
            return;
        }

        // Ulozime si stav klavesy, kterou prave drzime, pokud jsme ji prave stiskli
        key_states[event.key] = true;

        // Podle klavesy posleme do WebSocket kyzenou zpravu
        switch (event.key) {
            case "ArrowUp":
                socket.send("FORWARD");
                break;
            case "ArrowDown":
                socket.send("BACKWARD");
                break;
            case "ArrowLeft":
                socket.send("LEFT");
                break;
            case "ArrowRight":
                socket.send("RIGHT");
                break;
        }
    }
}

// Funkce pro zpracovani uvoleni klavesy
function onKeyUp(event) {
    // Pokud jsme prave uvolnili nekterou ze smerovych klaves
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(event.key)) {
        // Posleme do WebSocketu povel STOP pro zastaveni motoru pri jizde vpred/vzad
        if (["ArrowUp", "ArrowDown"].includes(event.key)) {
            socket.send("STOP");
        }
        // Zmenime stav klavesy v pomocne promenne key_states 
        key_states[event.key] = false;
    }
}

// Funkce pro akutalizaci historie
// Historie ma fixni delku, takze pri pridani novych souradnic
// se smazou ty nejstarsi
function aktualizujHistorii() {
    for (let i = history_x.length - 1; i > 0; i--) {
        history_x[i] = history_x[i-1];
        history_y[i] = history_y[i-1];
    }
    history_x[0] = x;
    history_y[0] = y;
}

// Po nacteni stranky zavolame funkci setup
// Fakticky zacatek naseho programu
window.addEventListener("load", setup);
)js";
